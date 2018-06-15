set style data dots
#set title 'Voltage drop of floating gate current output vs time'
set terminal pdf size 14cm,10cm
set output "dcurrent.pdf"
set notitle
set style fill transparent solid 0.1
unset ylabel 
set ytics 0.2
unset xlabel
set lmargin 8
set autoscale y
set xrange [0:10]
unset xtics
set multiplot
set origin 0,0.4
set size 1,0.6 
plot 'log.dat' using ($1/3600):($48-$49):($48+$49) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):48 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($44-$45):($44+$45) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):44 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($40-$41):($40+$41) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):40 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($36-$37):($36+$37) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):36 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($32-$33):($32+$33) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):32 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($28-$29):($28+$29) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):28 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($24-$25):($24+$25) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):24 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($20-$21):($20+$21) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):20 with lines lc rgb "black" notitle,  'log.dat' using ($1/3600):($16-$17):($16+$17) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):16 with lines lc rgb "black" notitle,  'log.dat' using ($1/3600):($12-$13):($12+$13) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):12 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($8-$9):($8+$9) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):8 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($4-$5):($4+$5) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):4 with lines lc rgb "black" notitle 
set size 1,0.42
set origin 0,0
set ylabel 'voltage[V] ' offset 1,5
set yrange [0.52:0.60]
set ytics 0.02
set xlabel 'time[h]'
set xtics nomirror out
replot
unset multiplot
set terminal pop
#set terminal pdf
#set output "dcurrent.pdf"
#replot
#set output "dcurrentzoom.pdf"
#replot
#set autoscale y
#set terminal  pop
