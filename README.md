# Cloth Simulation Project Report

This report covers the implementation details of our project, output(s) and a couple of observations and comments we'd like to bring to your notice.

# Implementation Detail

<h3>How to design the cloth?</h3>

A majority of our time went into deciding how we would design our cloth given the variety of methods out there to design a cloth.  The broad categorization looks like:

 - Geometric Techniques
	 - Connected catenary curves followed with relaxation passes
	 - Wrinkle model
 - Physical Techniques
	 - Elasticity Based Methods
	 - Particle Based Methods
	 - Mass – Spring Damper Model

We have chosen the **Mass – Spring Damper Model** for this project.

<h3>How does the Mass-Spring Damper Model work?</h3>

The **Mass – Spring Damper Model** works by assuming the cloth to be a connected set of points forming a grid. Following this, all the points are connected among themselves through a series of springs of $3$ categories:

 1. **Structural Springs:** Handle extension and compression and are connected vertically and horizontally.
 2. **Shear springs:** Handle shear stresses and are connected diagonally.
 3. **Bend springs:** Handle bending stresses and are connected vertically and horizontally to every other particle

# References

 - https://nccastaff.bournemouth.ac.uk/jmacey/OldWeb/MastersProjects/Msc05/cloth_simulation.pdf
 - 