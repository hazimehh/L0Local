#include "Grid.h"
#include "Grid1D.h"
#include "Grid2D.h"
#include "Normalize.h"
#include <memory>

// Assumes PG.P.Specs have been already set
Grid::Grid(const arma::mat& X, const arma::vec& y, const GridParams& PGi)
{
    PG = PGi;
    std::tie(BetaMultiplier, meanX, meany) = Normalize(X, y, Xscaled, yscaled, !PG.P.Specs.Classification, PG.intercept);
}

void Grid::Fit()
{

    std::vector< std::vector<std::unique_ptr<FitResult> > > G;
    if (PG.P.Specs.L0)
    {
        G.push_back(std::move(Grid1D(Xscaled, yscaled, PG).Fit()));
        Lambda12.push_back(0);
    }
    else
    {
        G = std::move(Grid2D(Xscaled, yscaled, PG).Fit());
    }

    Lambda0 = std::vector< std::vector<double> >(G.size());
    NnzCount = std::vector< std::vector<unsigned int> >(G.size());
    Solutions = std::vector< std::vector<arma::sp_mat> >(G.size());
    Intercepts = std::vector< std::vector<double> >(G.size());
    Converged = std::vector< std::vector<bool> >(G.size());



    //for (auto &g : G)
    for (unsigned int i=0; i<G.size(); ++i)
    {
        if (PG.P.Specs.L0L1)
        { Lambda12.push_back(G[i][0]->ModelParams[1]); }
        else if (PG.P.Specs.L0L2)
        { Lambda12.push_back(G[i][0]->ModelParams[2]); }

        for (auto &g : G[i])
        {

            Lambda0[i].push_back(g->ModelParams[0]);

            NnzCount[i].push_back(g->B.n_nonzero);

            if (g->IterNum != PG.P.MaxIters)
            {
                Converged[i].push_back(true);
            }
            else
            {
                Converged[i].push_back(false);
            }

            arma::sp_mat B_unscaled;
            double intercept;


            if (PG.P.Specs.Classification)
            {
                std::tie(B_unscaled, intercept) = DeNormalize(g->B, BetaMultiplier, meanX, meany);
                Solutions[i].push_back(B_unscaled);
                Intercepts[i].push_back(g->intercept + intercept); // + needed
            }

            else
            {
                std::tie(B_unscaled, intercept) = DeNormalize(g->B, BetaMultiplier, meanX, meany);
                Solutions[i].push_back(B_unscaled);
                Intercepts[i].push_back(intercept);
            }


        }

    }

}

/*
// Bypass for R interface
int main(){
	std::cout<<"Enter Method"<<std::endl;
	std::string method;
	std::cin>>method;
	arma::mat X;
	X.load("X_training.csv");

	arma::vec y;
	y.load("y_training.csv");

	GridParams PG;
	PG.Lambda2Max = 0.001;
	PG.Lambda2Min = 0.001;
	PG.Type = method; //Classification
	PG.P.Tol = 1e-4;
	PG.P.ActiveSetNum = 2;
	PG.G_nrows = 1;
	PG.G_ncols = 100;
	PG.NnzStopNum = 200;
	PG.P.ScreenSize = 100;

	auto start = std::chrono::steady_clock::now();

	auto g = Grid(X,y, PG);
	g.Fit();

	auto end = std::chrono::steady_clock::now();

	auto diff = end-start;

	std::cout << std::chrono::duration <double, std::milli> (diff).count() << " ms" << std::endl;

	//for(auto &sol:g.Solutions){sol.print();}


	return 0;

}
*/
