#pragma once
#include <cmath>

constexpr double UniversalGravitationalConstant = 6.674e-11; // 6.674 * 10^-11 N·m^2/kg^2
constexpr double Pi = 3.14159265358979323846; //value for Pi for compilers that aren't C++ 20
// Seconds per day and per Julian year (365.25 days)
constexpr double SecondsPerDay = 86400;
constexpr double DaysPerYear = 365.25;
constexpr double SecondsPerYear =  SecondsPerDay * DaysPerYear;

const int errDivisionZero = -1;
const int errRootNegativeNumber = -2;
const int errNumTooLarge = -3;

//Represents position of 3D coordinates
struct Point { 
	double x = 0;
	double y = 0;
	double z = 0;

	Point() = default;

	Point(double px, double py, double pz) {
		x = px;
		y = py;
		z = pz;
	}

	~Point() {}
};


// Validate numbers that can break calculations
static int validateDouble(double value) {
	if (value == 0)
		return errDivisionZero;
	else if (value < 0)
		return errRootNegativeNumber;
	return 0;
}

static double ScalarGravitationalForce(double mass1, double mass2, double radius) {
	/*This uses Newton's law of universal gravitation to calculate attraction force as a scalar 
	F = G * ( (m1* m2) / r^2)
	Where G is universal gravitational constant, m1 and m2 are masses, r is distance between centres of two objects
	The Force is returned in Newtons, with m1 and m2 being in kilograms and r being in metres
	*/
	int iErr = 0;
	if (iErr = validateDouble(radius))
		return iErr;

	double force = UniversalGravitationalConstant * ( (mass1 * mass2) / (radius * radius));
	return force;
}

/*
The following formulas for planetary motion are based on 
1. Newton's law of universal gravitational attraction
2. Kepler's laws of planetary attraction
They return only scalar values.
Note that:
- All calculations are done in standard units (kg, N, m, seconds).
- Note that the maximum size of a double datatype is 1.7976931348623157 * 10 ^308
- and the minimum size of a double datatype is -1.7976931348623157 * 10 ^308
- and the smallest possible value is 2.225 * 10 ^-308
- So all planetary calculations should involve doubles
*/

static double ScalarOrbitalVelocity(double bodyMass, double radius) {
	//This calculates the circular orbital velocity of an object around a planet or star
	//v = sqrt( (G *M) / r)
	int iErr = 0;
	if (iErr = validateDouble(radius))
		return iErr;

	double orbitalVelocity = std::sqrt( (UniversalGravitationalConstant * bodyMass) / radius);
	return orbitalVelocity;
}

static double ScalarGravitationalAroundBody(double bodyMass, double radius) {
	//This calculates the acceleration of an object around a planet or star
	//a = GM / (r^2)
	int iErr = 0;
	if (iErr = validateDouble(radius))
		return iErr;
	double acceleration = (UniversalGravitationalConstant * bodyMass) / (radius * radius);
	return acceleration;
}

static double OrbitalPeriod(double starMass, double acceleration) {
	//This calculates the time taken to complete one orbit in seconds
	//T = 2pi * sqrt( (a ^ 3) / GM )
	int iErr = 0;
	if (iErr = validateDouble(starMass))
		return iErr;
	if (iErr = validateDouble(acceleration))
		return iErr;

	double orbitalPeriod = 2 * Pi * std::sqrt((std::pow(acceleration, 3) / (UniversalGravitationalConstant * starMass)));
	return orbitalPeriod;
}

static double OrbitalPeriodInYears(double orbitalPeriod) {
	// Convert orbital period from seconds to years.
	// Using Julian year (365.25 days) => SecondsPerYear constant above.
	return orbitalPeriod / SecondsPerYear;
}

// Representation of a planet
struct PlanetaryBody {
	int index = 0;         // unique identifier
	double mass = 0.0;     // kilograms
	double radius = 0.0;   // metres (distance from centre to surface)
	Point position{};      // 3D position in metres

	PlanetaryBody() = default;

	PlanetaryBody(int idx, double m, double r, Point pos)
		: index(idx), mass(m), radius(r), position(pos) {
	}

	~PlanetaryBody() {}

	// Returns surface gravity (m/s^2) or a negative error code as double.
	double SurfaceGravity() const {
		return ScalarGravitationalAroundBody(mass, radius);
	}

	// Returns escape velocity from surface (m/s) or a negative error code as double.
	double EscapeVelocity() const {
		if (int iErr = validateDouble(radius))
			return iErr;
		if (int iErr = validateDouble(mass))
			return iErr;
		// v_escape = sqrt(2 * G * M / r)
		double escapeVelocity = std::sqrt(2.0 * UniversalGravitationalConstant * mass / radius);
		return escapeVelocity;
	}

	// Convenience: returns circular orbital velocity at given distance from central mass.
	// distanceFromCenter: orbital radius in metres. centralMass: mass of central body in kg.
	// Returns velocity (m/s) or negative error code as double.
	double OrbitalVelocityAt(double distanceFromCenter, double centralMass) const {
		if (int iErr = validateDouble(distanceFromCenter))
			return iErr;
		return ScalarOrbitalVelocity(centralMass, distanceFromCenter);
	}
};