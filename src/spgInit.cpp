/**********************************************************************
  spgInit.cpp - Functions for spacegroup initizialization.

  Copyright (C) 2015 - 2016 by Patrick S. Avery

  This source code is released under the New BSD License, (the "License").

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

 ***********************************************************************/

#include "elemInfo.h"

#include "spgInit.h"
#include "spgInitCombinatorics.h"
#include "wyckoffDatabase.h"
#include "fillCellDatabase.h"
#include "utilityFunctions.h"

// For getRandDouble()
#include "rng.h"

// For FunctionTracker
#include "functionTracker.h"

#include <cassert>
#include <fstream>
#include <tuple>
#include <iostream>

// Define these for debug output
//#define SPGINIT_DEBUG
//#define SPGINIT_WYCK_DEBUG

// Uncomment the right side of this line to output function starts and endings
#define START_FT //FunctionTracker functionTracker(__FUNCTION__);

using namespace std;

// Check if all the multiplicities of a spacegroup are even
static inline bool spgMultsAreAllEven(uint spg)
{
  START_FT;
  wyckoffPositions wyckVector = SpgInit::getWyckoffPositions(spg);
  // An error message should already be printed if this returns false
  if (wyckVector.size() == 0) return false;

  for (size_t i = 0; i < wyckVector.size(); i++) {
    if (numIsOdd(SpgInit::getMultiplicity(wyckVector.at(i)))) return false;
  }
  return true;
}

vector<numAndType> SpgInit::getNumOfEachType(const vector<uint>& atoms)
{
  START_FT;
  vector<uint> atomsAlreadyCounted;
  vector<numAndType> numOfEachType;
  for (size_t i = 0; i < atoms.size(); i++) {
    size_t size = 0;
    // If we already counted this one, just continue
    if (std::find(atomsAlreadyCounted.begin(), atomsAlreadyCounted.end(),
                  atoms.at(i)) != atomsAlreadyCounted.end()) continue;
    for (size_t j = 0; j < atoms.size(); j++)
      if (atoms.at(j) == atoms.at(i)) size++;
    numOfEachType.push_back(make_pair(size, atoms.at(i)));
    atomsAlreadyCounted.push_back(atoms.at(i));
  }
  // Sort from largest to smallest
  sort(numOfEachType.begin(), numOfEachType.end(), greaterThan);
  return numOfEachType;
}

// A unique position is a position that has no x, y, or z in it
bool SpgInit::containsUniquePosition(const wyckPos& pos)
{
  vector<string> xyzStrings = split(getWyckCoords(pos), ',');
  assert(xyzStrings.size() == 3);
  for (size_t i = 0; i < xyzStrings.size(); i++)
    if (!isNumber(xyzStrings.at(i))) return false;
  return true;
}

// Returns true on success and false on failure
bool getNumberInFirstTerm(const string& s, double& result, size_t& len)
{
  len = 0;
  size_t i = 0;

  if (s.size() == 0) {
    cout << "Error in " << __FUNCTION__ << ": the size is 0\n";
    return false;
  }

  bool isNeg = false;
  if (s.at(i) == '-') {
    isNeg = true;
    i++;
    len++;
  }

  // If it's just a variable, then the number is 1 or -1
  if (s.at(i) == 'x' || s.at(i) == 'y' || s.at(i) == 'z') {
    result = (isNeg) ? -1 : 1;
    return true;
  }

  // Find the length of the number
  size_t j = i;
  while (j < s.size() && (isDigit(s.at(j)) || s.at(j) == '.')) {
    len++;
    j++;
  }

  if (len == 0) {
    cout << "Error in " << __FUNCTION__ << ": invalid syntax: " << s << "\n";
    return false;
  }

  double ret;
  // This will throw if it fails to read the number
  try {
    ret = stof(s.substr(i, len));
  }
  catch (...) {
    cout << "Error in " << __FUNCTION__ << ": invalid syntax: " << s << "\n";
    return false;
  }

  result = (isNeg) ? -1 * ret : ret;
  return true;
}

// This might be a little bit too long to be inline...
double SpgInit::interpretComponent(const string& component,
                                   double x, double y, double z)
{
  START_FT;

  if (component.size() == 0) {
    cout << "Error in SpgInit::interpretComponent(): component is empty!\n";
    return -1;
  }

  int i = 0;
  double result = 0;
  while (i < component.size()) {
    // We assume we are adding unless told otherwise
    if (component.at(i) == '+') i++;

    double numInFront = 0;
    size_t len = 0;
    if (!getNumberInFirstTerm(component.substr(i), numInFront, len)) {
      cout << "Error in " << __FUNCTION__ << " getting number in term.\n";
      return 0;
    }

    // Shift i so that it is past the length of the number
    i += len;
    // If we reached the end, add it and break
    if (i >= component.size()) {
      result += numInFront;
      break;
    }

    // Figure out which variable we are dealing with...
    // or if we are dealing with one at all...
    switch (component.at(i)) {
      case 'x':
        result += numInFront * x;
        i++;
        break;
      case 'y':
        result += numInFront * y;
        i++;
        break;
      case 'z':
        result += numInFront * z;
        i++;
        break;
      default:
        result += numInFront;
        break;
    }
  }

  return result;
}

const wyckoffPositions& SpgInit::getWyckoffPositions(uint spg)
{
  START_FT;
  if (spg < 1 || spg > 230) {
    cout << "Error. getWyckoffPositions() was called for a spacegroup "
         << "that does not exist! Given spacegroup is " << spg << endl;
    return wyckoffPositionsDatabase.at(0);
  }

  return wyckoffPositionsDatabase.at(spg);
}

const fillCellInfo& SpgInit::getFillCellInfo(uint spg)
{
  if (spg < 1 || spg > 230) {
    cout << "Error. getFillCellInfo() was called for a spacegroup "
         << "that does not exist! Given spacegroup is " << spg << endl;
    return fillCellVector.at(0);
  }
  return fillCellVector.at(spg);
}

vector<string> SpgInit::getVectorOfDuplications(uint spg)
{
  fillCellInfo fcInfo = getFillCellInfo(spg);
  string duplicateString = fcInfo.first;
  vector<string> ret = splitAndRemoveParenthesis(duplicateString);
  // 0,0,0 should always be the first duplicate -- i. e. identiy
  ret.insert(ret.begin(),"0,0,0");
  return ret;
}

vector<string> SpgInit::getVectorOfFillPositions(uint spg)
{
  fillCellInfo fcInfo = getFillCellInfo(spg);
  string positionsString = fcInfo.second;
  vector<string> ret = splitAndRemoveParenthesis(positionsString);
  return ret;
}

bool SpgInit::addWyckoffAtomRandomly(Crystal& crystal, wyckPos& position,
                                     uint atomicNum, uint spg, int maxAttempts)
{
  START_FT;
#ifdef SPGINIT_WYCK_DEBUG
  cout << "At beginning of addWyckoffAtomRandomly(), atom info is:\n";
  crystal.printAtomInfo();
  cout << "Attempting to add an atom of atomicNum " << atomicNum
       << " at position " << getWyckCoords(position) << "\n";
#endif

  // If this contains a unique position, we only need to try once
  // Otherwise, we'd be repeatedly trying the same thing...
  if (containsUniquePosition(position)) {
    maxAttempts = 1;
  }

  int i = 0;
  bool success = false;
  do {
    // Generate random coordinates in the wyckoff position
    // Numbers are between 0 and 1
    double x = getRandDouble(0,1);
    double y = getRandDouble(0,1);
    double z = getRandDouble(0,1);

    vector<string> components = split(getWyckCoords(position), ',');

    // Interpret the three components of the Wyckoff position coordinates...
    double newX = interpretComponent(components[0], x, y, z);
    double newY = interpretComponent(components[1], x, y, z);
    double newZ = interpretComponent(components[2], x, y, z);

    // interpretComponenet() returns -1 if it failed to read the component
    if (newX == -1 || newY == -1 || newZ == -1) {
      cout << "addWyckoffAtomRandomly() failed due to a component not being "
           << "read successfully!\n";
      return false;
    }

    atomStruct newAtom(atomicNum, newX, newY, newZ);
    crystal.addAtom(newAtom);

    // Check the interatomic distances
    if (crystal.areIADsOkay(newAtom)) {
      // Now try to fill the cell using this new atom
      if (crystal.fillCellWithAtom(spg, newAtom)) success = true;
    }
    if (!success) {
      // Remove this atom and try again
      crystal.removeAtom(newAtom);
    }

    i++;
  } while (i < maxAttempts && !success);

  if (!success) return false;

#ifdef SPGINIT_WYCK_DEBUG
    cout << "After an atom with atomic num " << atomicNum << " was added and "
         << "the cell filled, the following is the atom info:\n";
    crystal.printAtomInfo();
#endif

  return true;
}

Crystal SpgInit::spgInitCrystal(uint spg,
                                const vector<uint>& atoms,
                                const latticeStruct& latticeMins,
                                const latticeStruct& latticeMaxes,
                                double minIADScalingFactor,
                                int numAttempts)
{
  START_FT;
  // First let's get a lattice...
  latticeStruct st = generateLatticeForSpg(spg, latticeMins, latticeMaxes);

  ElemInfo::applyScalingFactor(minIADScalingFactor);

  // Make sure it's a valid lattice
  if (st.a == 0 || st.b == 0 || st.c == 0 ||
      st.alpha == 0 || st.beta == 0 || st.gamma == 0) {
    cout << "Error in SpgInit::spgInitXtal(): an invalid lattice was "
         << "generated.\n";
    return Crystal();
  }

  systemPossibilities possibilities = SpgInitCombinatorics::getSystemPossibilities(spg, atoms);

  if (possibilities.size() == 0) {
    cout << "Error in SpgInit::" << __FUNCTION__ << "(): this spg '" << spg
         << "' cannot be generated with this composition\n";
    return Crystal();
  }

  //SpgInitCombinatorics::printSystemPossibilities(possibilities);
  // If we desire verbose output, print the system possibility to the log file
  if (e_verbosity == 'v')
    appendToLogFile(SpgInitCombinatorics::getSystemPossibilitiesString(possibilities));

  for (size_t i = 0; i < numAttempts; i++) {
    atomAssignments assignments = SpgInitCombinatorics::getRandomAtomAssignments(possibilities);

    //printAtomAssignments(assignments);
    // If we desire any output, print the atom assignments to the log file
    if (e_verbosity == 'r' || e_verbosity == 'v')
      appendToLogFile(getAtomAssignmentsString(assignments));

    if (assignments.size() == 0) {
      cout << "Error in SpgInit::spgInitXtal(): atoms were not successfully"
           << " assigned positions in assignAtomsToWyckPos()\n";
      continue;
    }

#ifdef SPGINIT_DEBUG
    cout << "\natomAssignments are the following (atomicNum, wyckLet, wyckPos):"
         << "\n";
    for (size_t j = 0; j < assignments.size(); j++)
      cout << "  " << assignments.at(j).second << ", "
           << getWyckLet(assignments.at(j).first)
           << ", " << getWyckCoords(assignments.at(j).first) << "\n";
    cout << "\n";
#endif

    Crystal crystal(st);
    bool assignmentsSuccessful = true;
    for (size_t j = 0; j < assignments.size(); j++) {
      wyckPos pos = assignments.at(j).first;
      uint atomicNum = assignments.at(j).second;
      if (!addWyckoffAtomRandomly(crystal, pos, atomicNum, spg)) {
        assignmentsSuccessful = false;
      }
    }

    // If we succeeded, return the crystal!
    if (assignmentsSuccessful) {
      appendToLogFile("*** Success! ***\n");
      return crystal;
    }
    else {
      if (e_verbosity == 'r' || e_verbosity == 'v') {
        stringstream ss;
        ss << "Failed to add atoms to satisfy MinIAD.\nObtaining new atom "
           << "assignments and trying again. Failure count: " << i + 1 << "\n\n";
        appendToLogFile(ss.str());
      }
      continue;
    }
  }

  // If we made it here, we failed to generate the crystal
  cout << "After " << numAttempts << " attempts: failed to generate "
       << "a crystal of spg " << spg << ".\n";
  return Crystal();
}

bool SpgInit::isSpgPossible(uint spg, const vector<uint>& atoms)
{
  START_FT;

  if (spg < 1 || spg > 230) return false;

  // Add in a test here to shorten the time checking if a spg is possible
  // If a spacegroup contains all even number multiplicities (many do),
  // and there is an atom with an odd amount, then that spacegroup is not
  // not possible
  vector<numAndType> numOfEachType = getNumOfEachType(atoms);
  bool containsOdd = false;
  for (size_t i = 0; i < numOfEachType.size(); i++) {
    if (numIsOdd(numOfEachType.at(i).first)) {
      containsOdd = true;
      break;
    }
  }

  if (containsOdd && spgMultsAreAllEven(spg)) return false;

  // If the test failed, we must just try to assign atoms and see if it works
  // The third boolean parameter is telling it to find only one combination
  // This speeds it up significantly
  if (SpgInitCombinatorics::getSystemPossibilities(spg, atoms,
                                                   true, false).size() == 0)
    return false;

  return true;
}

template <typename T>
static inline T getSmallest(const T& a, const T& b, const T& c)
{
  if (a <= b && a <= c) return a;
  else if (b <= a && b <= c) return b;
  else return c;
}

template <typename T>
static inline T getLargest(const T& a, const T& b, const T& c)
{
  if (a >= b && a >= c) return a;
  else if (b >= a && b >= c) return b;
  else return c;
}

latticeStruct SpgInit::generateLatticeForSpg(uint spg,
                                             const latticeStruct& mins,
                                             const latticeStruct& maxes)
{
  START_FT;

  latticeStruct st;
  if (spg < 1 || spg > 230) {
    cout << "Error: " << __FUNCTION__ << " was called for a "
         << "non-real spacegroup: " << spg << endl;
    // latticeStruct is initialized to have all "0" values. So just return a
    // "0" struct
    return st;
  }

  // Triclinic!
  else if (spg == 1 || spg == 2) {
    // There aren't really any constraints on a triclinic system...
    st.a     = getRandDouble(mins.a,     maxes.a);
    st.b     = getRandDouble(mins.b,     maxes.b);
    st.c     = getRandDouble(mins.c,     maxes.c);
    st.alpha = getRandDouble(mins.alpha, maxes.alpha);
    st.beta  = getRandDouble(mins.beta,  maxes.beta);
    st.gamma = getRandDouble(mins.gamma, maxes.gamma);
    return st;
  }

  // Monoclinic!
  else if (3 <= spg && spg <= 15) {
    // I am making beta unique here. This may or may not be the right angle
    // to make unique for the wyckoff positions in the database...

    // First make sure we can make alpha and gamma 90 degrees
    if (mins.alpha > 90 || maxes.alpha < 90 ||
        mins.gamma > 90 || maxes.gamma < 90) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains alpha and gamma to be 90 degrees. The "
           << "input min and max values for the alpha and gamma do not allow "
           << "this. Please change their min and max values.\n";
      return st;
    }

    st.a     = getRandDouble(mins.a,     maxes.a);
    st.b     = getRandDouble(mins.b,     maxes.b);
    st.c     = getRandDouble(mins.c,     maxes.c);
    st.alpha = st.gamma = 90.0;
    st.beta  = getRandDouble(mins.beta,  maxes.beta);
    return st;
  }

  // Orthorhombic!
  else if (16 <= spg && spg <= 74) {
    // For orthorhombic, all angles must be 90 degrees.
    // Check to see if we can make 90 degree angles
    if (mins.alpha > 90 || maxes.alpha < 90 ||
        mins.beta  > 90 || maxes.beta  < 90 ||
        mins.gamma > 90 || maxes.gamma < 90) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains all the angles to be 90 degrees. The "
           << "input min and max values for the angles do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    st.a     = getRandDouble(mins.a,     maxes.a);
    st.b     = getRandDouble(mins.b,     maxes.b);
    st.c     = getRandDouble(mins.c,     maxes.c);
    st.alpha = st.beta = st.gamma = 90.0;
    return st;
  }

  // Tetragonal!
  else if (75 <= spg && spg <= 142) {
    // For tetragonal, all angles must be 90 degrees.
    // Check to see if we can make 90 degree angles
    if (mins.alpha > 90 || maxes.alpha < 90 ||
        mins.beta  > 90 || maxes.beta  < 90 ||
        mins.gamma > 90 || maxes.gamma < 90) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains all the angles to be 90 degrees. The "
           << "input min and max values for the angles do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    // a and b need to be able to be the same number
    // Find the larger min and smaller max of each
    double largerMin = (mins.a > mins.b) ? mins.a : mins.b;
    double smallerMax = (maxes.a < maxes.b) ? maxes.a : maxes.b;

    if (largerMin > smallerMax) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains a and b to be equal. The "
           << "input min and max values for a and b do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    st.a     = st.b = getRandDouble(largerMin, smallerMax);
    st.c     = getRandDouble(mins.c, maxes.c);
    st.alpha = st.beta = st.gamma = 90.0;
    return st;
  }

  // Trigonal!
  // TODO: we are assuming here that all trigonal crystals can be
  // represented with hexagonal axes (and we are ignoring rhombohedral axes).
  // Is this correct?
  else if (143 <= spg && spg <= 167) {
    // For trigonal, alpha and beta must be 90 degrees, and gamma must be 120
    // Check to see if we can make these angles
    if (mins.alpha > 90 || maxes.alpha < 90 ||
        mins.beta  > 90 || maxes.beta  < 90) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains alpha and beta to be 90 degrees. The "
           << "input min and max values for the angles do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    if (mins.gamma > 120 || maxes.gamma < 120) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains gamma to be 120 degrees. The "
           << "input min and max values for gamma do not allow this. "
           << "Please change the min and max values.\n";
      return st;
    }

    // a and b need to be able to be the same number
    // Find the larger min and smaller max of each
    double largerMin = (mins.a > mins.b) ? mins.a : mins.b;
    double smallerMax = (maxes.a < maxes.b) ? maxes.a : maxes.b;

    if (largerMin > smallerMax) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains a and b to be equal. The "
           << "input min and max values for a and b do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    st.a     = st.b = getRandDouble(largerMin, smallerMax);
    st.c     = getRandDouble(mins.c, maxes.c);
    st.alpha = st.beta = 90.0;
    st.gamma = 120.0;
    return st;
  }

  // Hexagonal!
  // Note that this is identical to trigonal since in trigonal we are using
  // hexagonal axes.
  else if (168 <= spg && spg <= 194) {
    // For hexagonal, alpha and beta must be 90 degrees, and gamma must be 120
    // Check to see if we can make these angles
    if (mins.alpha > 90 || maxes.alpha < 90 ||
        mins.beta  > 90 || maxes.beta  < 90) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains alpha and beta to be 90 degrees. The "
           << "input min and max values for the angles do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    if (mins.gamma > 120 || maxes.gamma < 120) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains gamma to be 120 degrees. The "
           << "input min and max values for gamma do not allow this. "
           << "Please change the min and max values.\n";
      return st;
    }

    // a and b need to be able to be the same number
    // Find the larger min and smaller max of each
    double largerMin = (mins.a > mins.b) ? mins.a : mins.b;
    double smallerMax = (maxes.a < maxes.b) ? maxes.a : maxes.b;

    if (largerMin > smallerMax) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains a and b to be equal. The "
           << "input min and max values for a and b do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    st.a     = st.b = getRandDouble(largerMin, smallerMax);
    st.c     = getRandDouble(mins.c, maxes.c);
    st.alpha = st.beta = 90.0;
    st.gamma = 120.0;
    return st;
  }

  // Cubic!
  /*   ______
      /     /|
     /_____/ |
     |     | |
     |     | /
     |_____|/
  */
  else if (spg >= 195) {
    // We need to make sure that 90 degrees is an option and that a, b, and c
    // can be equal. Otherwise, it is impossible to generate this lattice
    if (mins.alpha > 90 || maxes.alpha < 90 ||
        mins.beta  > 90 || maxes.beta  < 90 ||
        mins.gamma > 90 || maxes.gamma < 90) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains all the angles to be 90 degrees. The "
           << "input min and max values for the angles do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }
    // Can a, b, and c be equal?
    // Find the greatest min value and the smallest max value
    double largestMin = getLargest<double>(mins.a, mins.b, mins.c);
    double smallestMax = getSmallest<double>(maxes.a, maxes.b, maxes.c);

    // They can't be equal!
    if (largestMin > smallestMax) {
      cout << "Error: " << __FUNCTION__ << " was called for a spacegroup of "
           << spg << " which constrains a, b, and c to be equal. The "
           << "input min and max values for a, b, and c do not allow this. "
           << "Please change their min and max values.\n";
      return st;
    }

    // If we made it this far, we can set up the cell!
    st.alpha = st.beta = st.gamma = 90.0;
    st.a = st.b = st.c = getRandDouble(largestMin, smallestMax);

    return st;
  }

  // We shouldn't get to this point because one of the if statements should have
  // worked...
  cout << "Error: " << __FUNCTION__ << " has a problem identifying spg " << spg
       << "\n";
  return st;
}

string SpgInit::getAtomAssignmentsString(const atomAssignments& a)
{
  stringstream s;
  s << "printing atom assignments:\n";
  s << "Atomic num : Wyckoff letter\n";
  for (size_t i = 0; i < a.size(); i++) {
    s << a.at(i).second << " : " << getWyckLet(a.at(i).first) << "\n";
  }
  return s.str();
}

void SpgInit::printAtomAssignments(const atomAssignments& a)
{
  cout << getAtomAssignmentsString(a);
}

// The name of the log file is available in the header as an extern
void SpgInit::appendToLogFile(const std::string& text)
{
  fstream fs;
  fs.open(e_logfilename, std::fstream::out | std::fstream::app);

  if (!fs.is_open()) {
    cout << "Error opening log file, " << e_logfilename << ".\n"
         << "The program will keep running, but log info will not be written.\n";
    return;
  }

  fs << text;

  fs.close();
}
