/**********************************************************************
  spgInit.h - Header file for spacegroup initialization functions

  Copyright (C) 2015 - 2016 by Patrick S. Avery

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

 ***********************************************************************/

#ifndef SPG_INIT_H
#define SPG_INIT_H

#include <vector>
#include <tuple>
#include <utility>

#include "crystal.h"
#include "spgInitOptions.h"

// output file name
extern std::string e_logfilename;
// verbosity
extern char e_verbosity;

// wyckPos is a tuple of a char (representing the Wyckoff letter),
// an int (representing the multiplicity), and a string (that contains the first
// Wyckoff position)
typedef std::tuple<char, int, std::string> wyckPos;

// Each spacegroup has a variable number of wyckoff positions. So each
// spacegroup will have it's own vector of wyckoff positions.
typedef std::vector<wyckPos> wyckoffPositions;

// This assign an atom with a specific atomic number to be placed in a specific
// wyckoff position
// 'uint' is the atomic number
typedef std::pair<wyckPos, uint> atomAssignment;
// This is a vector of atom assignments
typedef std::vector<atomAssignment> atomAssignments;

// number of atoms and atomic number
typedef std::pair<uint, uint> numAndType;

typedef std::pair<std::string, std::string> fillCellInfo;

// Utility struct for creating input for the spgInitCrystal function
struct spgInitInput {
  uint spg;
  std::vector<uint> atoms;
  latticeStruct latticeMins;
  latticeStruct latticeMaxes;
  double IADScalingFactor;
  double minRadius;
  std::vector<std::pair<uint, double>> manualAtomicRadii;
  double minVolume;
  double maxVolume;
  std::vector<std::pair<uint, char>> forcedWyckAssignments;
  char verbosity;
  int maxAttempts;
  bool forceMostGeneralWyckPos; // If this is not true, then we are not
                                // guaranteed to get the right spg
  // Most basic constructor
  spgInitInput(uint _spg, const std::vector<uint>& _atoms,
               const latticeStruct& _lmins,
               const latticeStruct& _lmaxes) :
                   spg(_spg),
                   atoms(_atoms),
                   latticeMins(_lmins),
                   latticeMaxes(_lmaxes),
                   IADScalingFactor(1.0),
                   minRadius(0.0),
                   manualAtomicRadii(std::vector<std::pair<uint, double>>()),
                   minVolume(-1.0),
                   maxVolume(-1.0),
                   forcedWyckAssignments(std::vector<std::pair<uint, char>>()),
                   verbosity('n'),
                   maxAttempts(100),
                   forceMostGeneralWyckPos(true) {}
  // Defining-everything constructor
  spgInitInput(uint _spg, const std::vector<uint>& _atoms,
               const latticeStruct& _lmins,
               const latticeStruct& _lmaxes,
               double _IADSF, double _minRadius,
               const std::vector<std::pair<uint, double>>& _mar,
               double _minVolume, double _maxVolume,
               std::vector<std::pair<uint, char>> _fwa,
               char _v, int _maxAttempts, bool _fmgwp) :
                   spg(_spg),
                   atoms(_atoms),
                   latticeMins(_lmins),
                   latticeMaxes(_lmaxes),
                   IADScalingFactor(_IADSF),
                   minRadius(_minRadius),
                   manualAtomicRadii(_mar),
                   minVolume(_minVolume),
                   maxVolume(_maxVolume),
                   forcedWyckAssignments(_fwa),
                   verbosity(_v),
                   maxAttempts(_maxAttempts),
                   forceMostGeneralWyckPos(_fmgwp) {}
};

class SpgInit {
 public:

  static char getWyckLet(const wyckPos& pos) {return std::get<0>(pos);};
  static uint getMultiplicity(const wyckPos& pos) {return std::get<1>(pos);};
  static std::string getWyckCoords(const wyckPos& pos) {return std::get<2>(pos);};

  /*
   * Obtain the wyckoff positions of a spacegroup from the database
   *
   * @param spg The spacegroup from which to obtain the wyckoff positions
   *
   * @return Returns a constant reference to a vector of wyckoff positions
   * for the spacegroup from the database in wyckoffDatabase.h. Returns an
   * empty vector if an invalid spg is entered.
   */
  static const wyckoffPositions& getWyckoffPositions(uint spg);

  static wyckPos getWyckPosFromWyckLet(uint spg, char wyckLet);

  static const fillCellInfo& getFillCellInfo(uint spg);

  static std::vector<std::string> getVectorOfDuplications(uint spg);

  static std::vector<std::string> getVectorOfFillPositions(uint spg);

  static double interpretComponent(const std::string& component,
                                          double x, double y, double z);

  /*
   * Used to determine if a spacegroup is possible for a given set of atoms.
   * It is determined by using the multiplicities in the Wyckoff database.
   *
   * @param spg The spacegroup to check.
   * @param atomTypes A vector of atomic numbers (one for each atom). So if
   *                  our system were Ti1O2, it should be {22, 8, 8}
   *
   * @return True if the spacegroup may be generated. False if it cannot.
   *
   */
  static bool isSpgPossible(uint spg, const std::vector<uint>& atomTypes);

  /*
   * Generates a latticeStruct (contains a, b, c, alpha, beta, and gamma as
   * doubles) with randomly generated parameters for a given
   * spacegroup, mins, and maxes. If a parameter must be constrained due
   * to the spacegroup, it will be.
   *
   * @param spg The spacegroup for which to generate a lattice.
   * @param mins The minimum values for the lattice parameters
   * @param maxes The maximum values for the lattice parameters
   *
   * @return Returns the lattice struct that was generated. Returns a struct
   * with all zero values if a proper lattice struct cannot be generated
   * for a specific spacegroup due to invalid mins or maxes.
   * An error message will be printed to stdout with information if it fails.
   */
  static latticeStruct generateLatticeForSpg(uint spg,
                                             const latticeStruct& mins,
                                             const latticeStruct& maxes);
  /*
   * Attempts to add an atom randomly to a wyckoff position of a given crystal.
   * The position of the atom is constrained by the given wyckoff position.
   * It will attempt to add an atom randomly to satisfy minimum IAD for
   * maxAttempts times, and if it fails, it returns false. If a fixed wyckoff
   * position is given, it will just add the atom to the fixed wyckoff position.
   * After it adds a Wyckoff atom, it attempts to fill the cell with it
   * according to the spacegroup. If it fails, it will remove the new Wyckoff
   * atom and try again.
   *
   * @param crystal The Crystal object for which an atom will be added
   * @param position The wyckoff position to add the atom to
   * @param atomicNum The atomic number of the atom to be added
   * @param spg The spacegroup which we are creating.
   * @param maxAttempts The number of attempts to make to add the atom randomly
   *                    before the function returns false. Default is 1000.
   *
   * @return True if it succeeded, and false if it failed.
   */
  static bool addWyckoffAtomRandomly(Crystal& crystal, wyckPos& position,
                                     uint atomicNum, uint spg,
                                     int maxAttempts = 1000);

  /*
   * Initialze and return a Crystal object with a given spacegroup!
   * The lattice mins and lattice maxes provide constraints for the lattice
   * to be generated. The list of atom types tell it which atoms to be added.
   * Returns a zero-volume Crystal object if it failed to generate it.
   *
   * @param spg The international number for the spacegroup to be generated
   * @param atomTypes A vector of atomic numbers (one for each atom). So if
   *                  our system were Ti1O2, it should be {22, 8, 8}
   * @param latticeMins A latticeStruct that contains the minima for a, b, c,
   *                    alpha, beta, and gamma.
   * @param latticeMaxes A latticeStruct that contains the maxima for a, b, c,
   *                     alpha, beta, and gamma.
   * @param IADScalingFactor A scaling factor used to scale the IAD
   * @param minVolume The minimum volume for the crystal to be generated.
   *                  If set to -1, there is no minimum volume.
   * @param maxVolume The maximum volume for the crystal to be generated.
   *                  If set to -1, there is no maximum volume.
   * @param numAttempts The max number number of attempts to generate a crystal
   *                    given these conditions. It will still only find all
   *                    combinations once (that's the most time consuming
   *                    step). It will randomly pick combinations for every
   *                    subsequent attempt from the combinations it found
   *                    originally.
   *
   * @return A Crystal object with the given spacegroup, atoms,
   * and lattice within the provided lattice constraints. Returns a Crystal
   * with zero volume if it failed to generate one successfully.
   */
  static Crystal spgInitCrystal(const spgInitInput& input);

  static std::vector<numAndType> getNumOfEachType(
                                   const std::vector<uint>& atoms);

  static bool containsUniquePosition(const wyckPos& pos);

  static std::string getAtomAssignmentsString(const atomAssignments& a);

  static void printAtomAssignments(const atomAssignments& a);

  static void appendToLogFile(const std::string& text);

};

#endif
