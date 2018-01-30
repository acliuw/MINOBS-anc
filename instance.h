#ifndef INSTANCE_H
#define INSTANCE_H 

#include <vector>
#include <unordered_set>
#include "variable.h"
#include "types.h"

typedef std::pair<int, int> Ancestral; // X --> Y is (X, Y)

class Instance {
  public:
    Instance(std::string fileName);
    int getN() const;
    int getM() const;
    bool isConstraint(int a, int b) const;

    Variable &getVar(int i);
    const Ancestral &getAncestral(int i) const;

    friend std::ostream& operator<<(std::ostream &os, const Instance& I);

  private:
    int n, m;
    std::vector<Variable> vars;
    std::vector<Ancestral> ancestralConstraints;
    std::unordered_set<int> orderConstraints;
};

#endif /* INSTANCE_H */
