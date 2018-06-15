import numpy
import sys
import pylab

enable_plot = True

clkperiod_bioms = 4.0*1.0e-9*1.0e4*1.0e3

infile = "tmag_timestamp_test_250.csv"
if (len(sys.argv) > 1):
	infile = sys.argv[1]

vect = numpy.loadtxt(infile)

# correct overflows
fpga_correction = 0
hicann_correction = 0
for nentry in range(len(vect)-1):

	if ((vect[nentry,0] > 30000)and(vect[nentry+1,0]<1000)): # overflow in FPGA timestamp
		vect[nentry,0] += fpga_correction
		fpga_correction += 32768
	else:
		vect[nentry,0] += fpga_correction

	if ((vect[nentry,3] > 30000)and(vect[nentry+1,3]<1000)): # overflow in HICANN timestamp
		vect[nentry,3] += hicann_correction
		hicann_correction += 32768
	else:
		vect[nentry,3] += hicann_correction

vect[-1,0] += fpga_correction
vect[-1,3] += hicann_correction

diff_vect = vect[:,0]-vect[:,3];

meanval = numpy.mean(diff_vect)
stddev = numpy.std(diff_vect)
mini = numpy.min(diff_vect)
maxi = numpy.max(diff_vect)

print "Statistics of timestamp difference:"
print "  Mean:               % 9.2f (biol. % 9.3fms)" % (meanval, meanval*clkperiod_bioms)
print "  Standard deviation: % 9.2f (biol. % 9.3fms)" % (stddev, stddev*clkperiod_bioms)
print "  Minimum:            % 9.2f (biol. % 9.3fms)" % (mini, mini*clkperiod_bioms)
print "  Maximum:            % 9.2f (biol. % 9.3fms)" % (maxi, maxi*clkperiod_bioms)
print "  Minimum from mean:  % 9.2f (biol. % 9.3fms)" % (mini-meanval, (mini-meanval)*clkperiod_bioms)
print "  Maximum from mean:  % 9.2f (biol. % 9.3fms)" % (maxi-meanval, (maxi-meanval)*clkperiod_bioms)

if (enable_plot):
	pylab.figure()
	pylab.plot(vect[:,3])
	pylab.title("HICANN timestamps, raw data")
	pylab.xlabel("pulse nr.")
	pylab.ylabel("clk cycles")

	pylab.figure()
	pylab.hist(diff_vect,30)
	pylab.title("FPGA-to-HICANN Timestamp difference, raw data")
	pylab.xlabel("Timestamp difference in clk cycles")
	pylab.ylabel("# of occurences")

	pylab.figure()
	pylab.hist((diff_vect-meanval)*clkperiod_bioms,30)
	pylab.title("FPGA-to-HICANN timestamp difference, shifted by mean, biological domain")
	pylab.xlabel("Resulting biological difference in ms")
	pylab.ylabel("# of occurences")

	pylab.show()

