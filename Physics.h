#pragma once
#include <math.h>

const int errDivisionZero = -1;

static double ScalarGravitationalForce(double mass1, double mass2, double radius) {
	/*This uses Newton's law of universal gravitation to calculate attraction force as a scalar 
	F = G * ( (m1* m2) / r^2)
	Where G is gravitational constant, m1 and m2 are masses, r is distance between centres of two objects
	The Force is returned in Newtons, with m1 and m2 being in kilograms and r being in metres
	*/
	const double GravitationalConstant = 0.00000000006674; //6.674 * 10^-11 N/m^2/kg^2
	if (radius == 0)
		return errDivisionZero;
	double force = GravitationalConstant * ( (mass1 * mass2) / (radius * radius));
	return force;
}
