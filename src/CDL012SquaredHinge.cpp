#include "CDL012SquaredHinge.h"

#include "Normalize.h" // Remove later
#include "Grid.h" // Remove later


CDL012SquaredHinge::CDL012SquaredHinge(const arma::mat& Xi, const arma::vec& yi, const Params& P) : CD(Xi, yi, P)
{
    //LipschitzConst = 0.25; // for logistic loss function only
    twolambda2 = 2 * ModelParams[2];
    qp2lamda2 = (LipschitzConst + twolambda2); // this is the univariate lipschitz const of the differentiable objective
    thr = std::sqrt((2 * ModelParams[0]) / qp2lamda2);
    lambda1 = ModelParams[1];
    lambda1ol = lambda1 / qp2lamda2;
    b0 = P.b0; // Initialize from previous later....!
    Xtr = P.Xtr; Iter = P.Iter; result.ModelParams = P.ModelParams;
    onemyxb = 1 - *y % (*X * B + b0);
    NoSelectK = P.NoSelectK;
}


inline double CDL012SquaredHinge::Derivativei(unsigned int i)
{
    //arma::vec ExpyXB = arma::exp(*y % (*X*B + b0));
    //return - arma::sum( (*y % X->unsafe_col(i)) / (1 + ExpyXB) ) + twolambda2 * B[i];
    //onemyxb = 1 - *y % (*X * B + b0);
    arma::uvec indices = arma::find(onemyxb > 0);
    return arma::sum(2 * onemyxb.elem(indices) % (- y->elem(indices) % X->unsafe_col(i).elem(indices))) + twolambda2 * B[i];
}

inline double CDL012SquaredHinge::Derivativeb()
{
    //arma::vec onemyxb = 1 - *y % (*X * B + b0);
    arma::uvec indices = arma::find(onemyxb > 0);
    return arma::sum(2 * onemyxb.elem(indices) % (- y->elem(indices) ) );
}

FitResult CDL012SquaredHinge::Fit()
{
    // arma::mat Xy = X->each_col() % *y; // later

    bool SecondPass = false;

    objective = Objective(r, B);

    for (unsigned int t = 0; t < MaxIters; ++t)
    {
        //std::cout<<"CDL012 Logistic: "<< t << " " << objective <<std::endl;
        Bprev = B;

        // Update the intercept
        double b0old = b0;
        double partial_b0 = Derivativeb();
        b0 -= partial_b0 / (n * LipschitzConst); // intercept is not regularized
        onemyxb += *y * (b0old - b0);
        //std::cout<<"Intercept: "<<b0<<" Obj: "<<Objective(r,B)<<std::endl;


        for (auto& i : Order)
        {

            // Calculate Partial_i
            double Biold = B[i];
            double partial_i = Derivativei(i);
            (*Xtr)[i] = std::abs(partial_i); // abs value of grad

            double x = Biold - partial_i / qp2lamda2;
            double z = std::abs(x) - lambda1ol;


            if (z >= thr || (i < NoSelectK && z>0)) 	// often false so body is not costly
            {
                //std::cout<<"z: "<<z<<" thr: "<<thr<<" Biold"<<Biold<<std::endl;
                double Bnew = std::copysign(z, x);
                B[i] = Bnew;
                onemyxb += (Biold - Bnew) * *y % X->unsafe_col(i);
                //std::cout<<"In. "<<Objective(r,B)<<std::endl;
            }

            else if (Biold != 0)   // do nothing if x=0 and B[i] = 0
            {
                B[i] = 0;
                onemyxb += (Biold) * *y % X->unsafe_col(i);
            }
        }

        if (Converged())
        {

            if (Stabilized == true && !SecondPass)
            {
                Order = OldOrder; // Recycle over all coordinates to make sure the achieved point is a CW-min.
                // SecondPass = true; // a 2nd pass will be performed
            }

            else
            {
                //std::cout<<"Converged in "<<t+1<<" iterations."<<std::endl;
                break;
            }

        }

        if (ActiveSet) {SupportStabilized();}

    }

    result.Objective = objective;
    result.B = B;
    result.Model = this;
    result.intercept = b0;
    result.IterNum = CurrentIters;
    return result;
}

inline double CDL012SquaredHinge::Objective(arma::vec & r, arma::sp_mat & B)   // hint inline
{

    auto l2norm = arma::norm(B, 2);
    //arma::vec onemyxb = 1 - *y % (*X * B + b0);
    arma::uvec indices = arma::find(onemyxb > 0);

    return arma::sum(onemyxb.elem(indices) % onemyxb.elem(indices)) + ModelParams[0] * B.n_nonzero + ModelParams[1] * arma::norm(B, 1) + ModelParams[2] * l2norm * l2norm;
}


/*
int main(){


	Params P;
	P.ModelType = "L012Logistic";
	P.ModelParams = std::vector<double>{18.36,0,0.01};
	P.ActiveSet = true;
	P.ActiveSetNum = 6;
	P.Init = 'z';
	P.MaxIters = 200;
	//P.RandomStartSize = 100;


	arma::mat X;
	X.load("X.csv");

	arma::vec y;
	y.load("y.csv");

	std::vector<double> * Xtr = new std::vector<double>(X.n_cols);
	P.Xtr = Xtr;

	arma::mat Xscaled;
	arma::vec yscaled;

	arma::vec BetaMultiplier;
	arma::vec meanX;
	double meany;

	std::tie(BetaMultiplier, meanX, meany) = Normalize(X,y, Xscaled, yscaled, false);


	auto model = CDL012Logistic(Xscaled, yscaled, P);
	auto result = model.Fit();

	arma::sp_mat B_unscaled;
	double intercept;
	std::tie(B_unscaled, intercept) = DeNormalize(result.B, BetaMultiplier, meanX, meany);

	result.B.print();
	B_unscaled.print();


	//std::cout<<result.intercept<<std::endl;
	//arma::sign(Xscaled*result.B + result.intercept).print();
	//std::cout<<"#############"<<std::endl;
	//arma::sign(X*B_unscaled + result.intercept).print();



	// GridParams PG;
	// PG.Type = "L0Logistic";
	// PG.NnzStopNum = 12;
  //
	// arma::mat X;
	// X.load("X.csv");
  //
	// arma::vec y;
	// y.load("y.csv");
	// auto g = Grid(X, y, PG);
  //
	// g.Fit();
  //
	// unsigned int i = g.NnzCount.size();
	// for (uint j=0;j<i;++j){
	// 	std::cout<<g.NnzCount[j]<<"   "<<g.Lambda0[j]<<std::endl;
	// }



	return 0;

}
*/
