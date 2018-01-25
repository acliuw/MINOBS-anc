#include "localsearch.h"
#include "debug.h"
#include <numeric>
#include <utility>
#include <deque>
#include <cmath>
#include "tabulist.h"
#include "util.h"
#include "movetabulist.h"
#include "swaptabulist.h"

LocalSearch::LocalSearch(Instance &instance) : instance(instance) { 
}

const ParentSet &LocalSearch::bestParent(const Ordering &ordering, const Types::Bitset pred, int idx) const {
  int current = ordering.get(idx);
  const Variable &v = instance.getVar(current);
  return bestParentVar(pred, v);
}

const ParentSet &LocalSearch::bestParentVar(const Types::Bitset pred, const Variable &v) const {
  int numParents = v.numParents();
  for (int i = 0; i < numParents; i++) {
    const ParentSet &p = v.getParent(i);
    if (p.subsetOf(pred)) {
      return p;
    }
  }
  //DBG("PARENT SET NOT FOUND");
  return v.getParent(0); //Should never happen in THeory
}


// Sketchy to use pointeres but we have to use null.. it's possible there is no Var at all.
const ParentSet *LocalSearch::bestParentVarWithParent(const Types::Bitset pred, const Variable &a, const Variable &b, const Types::Score orig) const {
  //const std::vector<int> &candidates = a.parentsWithVarId(b.getId());
  auto candidates_iter = a.parentsWithVarId(b.getId());
  if (candidates_iter == a.parentsWithVar.end()) return NULL;
  const std::vector<int> &candidates = candidates_iter->second;
  int n = candidates.size();
  for (int i = 0; i < n; i++) {
    const ParentSet &p = a.getParent(candidates[i]);
    if (p.getScore() >= orig) break;
    if (p.subsetOf(pred)) {
      return &p;
    }
  }
  //DBG("PARENT SET NOT FOUND");
  return NULL; //Coult happen
}


Types::Bitset LocalSearch::getPred(const Ordering &ordering, int idx) const {
  int n = instance.getN();
  Types::Bitset pred(n, 0);
  for (int i = 0; i < idx; i++) {
    pred[ordering.get(i)] = 1;
  }
  return pred;
}


bool LocalSearch::consistentWithAncestral(const Ordering &ordering) const {
  int n = instance.getN(), m = instance.getM();
  int pos[n];
  for (int i=0; i < n; i++) {
    pos[ordering.get(i)] = i;
  }

  for (int i=0; i < m; i++) {
    int x = instance.getAncestral(i).first, y = instance.getAncestral(i).second;
    if (pos[x] > pos[y]) {
      return false;
    }
  }

  return true;
}

// New code
Types::Score LocalSearch::getBestScoreWithParents(const Ordering &ordering, std::vector<int> &parents, std::vector<Types::Score> &scores) const {
  int n = instance.getN(), m = instance.getM();

  if (!consistentWithAncestral(ordering)) {
    return 223372036854775807LL;
  }

  
  Types::Bitset pred(n, 0);
  Types::Score score = 0;
  for (int i = 0; i < n; i++) {
    const ParentSet &p = bestParent(ordering, pred, i);
    parents[ordering.get(i)] = p.getId();
    scores[ordering.get(i)] = p.getScore();
    score += p.getScore();
    pred[ordering.get(i)] = 1;
  }

  if (m == 0) {
    return score;
  }
  return modifiedDAGScore(ordering, parents, scores);
}

// CAN BE OPTIMIZED
bool LocalSearch::hasDipath(const std::vector<int> &parents, int x, int y) const {
  if (x == y) {
    return true;
  }

  const Variable &yVar = instance.getVar(y);
  const ParentSet &p = yVar.getParent(parents[y]);
  const std::vector<int> &pars = p.getParentsVec();

  for (int i=0; i < pars.size(); i++) {
    if (hasDipath(parents, x, pars[i])) {
      return true;
    }
  }

  return false;
}

int LocalSearch::numConstraintsSatisfied(const std::vector<int> &parents) const {
  int n = instance.getN(), m = instance.getM(), count = 0;

  for (int i=0; i < m; i++) {
    const Ancestral &cons = instance.getAncestral(i);
    if (hasDipath(parents, cons.first, cons.second)) {
      count++;
    }
  }

  return count;
}


Types::Score LocalSearch::modifiedDAGScore(const Ordering &ordering, std::vector<int> &parents, std::vector<Types::Score> &scores) const {
  int n = instance.getN(), m = instance.getM();

  for (int iter = 0; iter < m * 3; iter++) {
    // Consider all feasible parent sets

    Types::Bitset pred(n, 0);
    double bestScore = 223372036854775807;
    std::vector<int> bestGraph = parents;
    int curNumSat = numConstraintsSatisfied(parents);

    //std::cout << "Constraints satisfied: " << curNumSat << std::endl;
    if (curNumSat == m) {
      // Compute the final score of the graph.

      Types::Score finalSc = 0LL;
      for (int i=0; i<n; i++) {
        finalSc += instance.getVar(i).getParent(parents[i]).getScore();
        scores[i] = instance.getVar(i).getParent(parents[i]).getScore();
      }

      return finalSc;
    }


    for (int i=0; i<n; i++) {
      int cur = ordering.get(i);
      const Variable &var = instance.getVar(cur);

      int numParents = var.numParents();
      for (int j=0; j < numParents; j++) {
        const ParentSet &p = var.getParent(j);

        if (j != parents[cur] && p.subsetOf(pred)) {
          int oldPar = parents[cur];
          parents[cur] = j;

          int numSat = numConstraintsSatisfied(parents) - curNumSat;
          Types::Score sc = p.getScore() - var.getParent(oldPar).getScore();

          double adjustedSc;
          if (numSat > 0) {
            adjustedSc = (double)sc / numSat;
          } else if (numSat == 0) {
            adjustedSc = (double)sc * 100000;
          } else {
            adjustedSc = 223372036854775807;
          }

          if (adjustedSc < bestScore) {
            bestScore = adjustedSc;
            bestGraph = parents;
          }

          parents[cur] = oldPar;

        } 
      }

      pred[cur] = 1;
    }

    parents = bestGraph;
  }


  return 223372036854775807LL;
}

Types::Score LocalSearch::findBestScoreRange(const Ordering &o, int start, int end) {
  int n = instance.getN();
  Types::Score curScore = 0;
  Types::Bitset used(n, 0);
  for (int i = 0; i < start; i++) {
    used[o.get(i)] = 1;
  }
  for (int i = start; i <= end; i++) {
    const ParentSet &cur = bestParent(o, used, i);
    curScore += cur.getScore();
    used[o.get(i)] = 1;
  }
  return curScore;
}

SearchResult LocalSearch::hillClimb(const Ordering &ordering) {
  bool improving = false;
  int n = instance.getN();
  std::vector<int> parents(n);
  std::vector<Types::Score> scores(n);
  int steps = 0;
  std::vector<int> positions(n);
  Ordering cur(ordering);

  Types::Score curScore = getBestScoreWithParents(cur, parents, scores);
  std::iota(positions.begin(), positions.end(), 0);
  DBG("Inits: " << cur);
  do {
    improving = false;
    std::random_shuffle(positions.begin(), positions.end());
    for (int s = 0; s < n && !improving; s++) {
      int pivot = positions[s];


      std::vector<int> bestParents(n);
      std::vector<Types::Score> bestScores(n);
      Ordering bestOrdering;
      Types::Score bestSc = 223372036854775807LL;

      for (int j=0; j < n; j++) {
        if (j == pivot) continue;

        std::vector<int> curParents(n);
        std::vector<Types::Score> curScores(n);
        cur.swap(pivot, j);
        Types::Score sc = getBestScoreWithParents(cur, curParents, curScores);

        if (sc < bestSc) {
          bestSc = sc;
          bestParents = curParents;
          bestScores = curScores;
          bestOrdering = cur;
        }

        cur.swap(pivot, j); // Undo the previous swap.
      }

      if (bestSc < curScore) {
        steps += 1;
        improving = true;
        cur = bestOrdering;
        parents = bestParents;
        scores = bestScores;
        curScore = bestSc;
      }



      /*
      FastPivotResult result = getBestInsertFast(cur, pivot, curScore, parents, scores);
      if (result.getScore() < curScore) {
        steps += 1;
        improving = true;
        cur.insert(pivot, result.getSwapIdx());
        parents = result.getParents();
        scores = result.getScores();
        curScore = result.getScore();
      }
      */
    }
    DBG("Cur Score: " << curScore);
  } while(improving);
  DBG("Total Steps: " << steps);
  return SearchResult(curScore, cur);
}

SearchResult LocalSearch::genetic(float cutoffTime, int INIT_POPULATION_SIZE, int NUM_CROSSOVERS, int NUM_MUTATIONS,
    int MUTATION_POWER, int DIV_LOOKAHEAD, int NUM_KEEP, float DIV_TOLERANCE, CrossoverType crossoverType, int greediness, Types::Score opt, ResultRegister &rr) {
  int n = instance.getN();
  SearchResult best(Types::SCORE_MAX, Ordering(n));
  std::deque<Types::Score> fitnesses;
  Population population(*this);
  int numGenerations = 1;
  std::cout << "Time: " << rr.check() << " Generating initial population" << std::endl;
  for (int i = 0; i < INIT_POPULATION_SIZE; i++) {
    SearchResult o;
    if (greediness == -1) {
      o = hillClimb(Ordering::randomOrdering(instance));
    } else {
      o = hillClimb(Ordering::greedyOrdering(instance, greediness));
    }
  /*
   * Added to show a solution early.
   */
    std::cout << "Time: " << rr.check() <<  " i = " << i << " The score is: " << o.getScore() << std::endl;
    std::vector<int> parents(n);
    std::vector<Types::Score> scores(n);

    rr.record(o.getScore(), o.getOrdering());
    population.addSpecimen(o);
  }
  std::cout << "Done generating initial population" << std::endl;
  do {
    //std::cout << "Time: " << rr.check() << " Starting generation " << numGenerations << std::endl;
    //DBG(population);
    std::vector<SearchResult> offspring;
    population.addCrossovers(NUM_CROSSOVERS, crossoverType, offspring);
    //DBG(population);
    population.mutate(NUM_MUTATIONS, MUTATION_POWER, offspring);
    //DBG(population);
    population.append(offspring);
    population.filterBest(INIT_POPULATION_SIZE);
    DBG(population);
    Types::Score fitness = population.getAverageFitness();
    fitnesses.push_back(fitness);
    if (fitnesses.size() > DIV_LOOKAHEAD) {
      Types::Score oldFitness = fitnesses.front();
      fitnesses.pop_front();
      float change = std::abs(((float)fitness-(float)oldFitness)/(float)oldFitness);
      if (change < DIV_TOLERANCE && DIV_TOLERANCE != -1) {
        DBG("Diversification Step. Change: " << change << " Old: " << oldFitness << " New: " << fitness);
        population.diversify(NUM_KEEP, instance);
        fitnesses.clear();
      }
    }
    DBG("Fitness: " << population.getAverageFitness());
    SearchResult curBest = population.getSpecimen(0);
    Types::Score curScore = curBest.getScore();
    if (curScore < best.getScore()) {
      std::cout << "Time: " << rr.check() <<  " The best score at this iteration is: " << curScore << std::endl;
      rr.record(curBest.getScore(), curBest.getOrdering());
      best = curBest;
    }
    numGenerations++;
  } while (rr.check() < cutoffTime);
  std::cout << "Generations: " << numGenerations << std::endl;
  return best;
}

void LocalSearch::checkSolution(const Ordering &o) {
  int n = instance.getN(), m = instance.getM();
  std::vector<int> parents(n);
  std::vector<Types::Score> scores(n);
  long long sc = getBestScoreWithParents(o, parents, scores);
  long long scoreFromScores = 0;
  long long scoreFromParents = 0;
  std::vector<int> inverse(n);
  for (int i = 0; i < n; i++) {
    inverse[o.get(i)] = i;
  }
  bool valid = true;
  for (int i = 0; i < n; i++) {
    int var = o.get(i);
    const Variable &v = instance.getVar(var);
    const ParentSet &p = v.getParent(parents[var]);
    const std::vector<int> &parentVars = p.getParentsVec();
    std::string parentsStr = "";
    bool before = true;
    for (int j = 0; j < parentVars.size(); j++) {
      parentsStr += std::to_string(parentVars[j]) + " ";
      before = before && (inverse[parentVars[j]] < i);
    }
    valid &= before;
    scoreFromParents += p.getScore();
    scoreFromScores += scores[var];
    std::cout << "Ordering[" << i << "]\t= "<< var << "\tScore:\t" << scores[var] << "\tParents:\t{ " << parentsStr << "}\tValid: " << before << std::endl;
  }

  std::cout << "Total Score: " << scoreFromScores << " " << scoreFromParents << " " << sc << std::endl;
  std::string validStr = valid ? "Good" : "Bad";
  std::cout << "Validity Check: " << validStr << std::endl;

  if (numConstraintsSatisfied(parents) == m) {
    std::cout << "Ancestral constraints check: Good" << std::endl;
  } else {
    std::cout << "Ancestral constraints check: Bad" << std::endl;
  }
}
