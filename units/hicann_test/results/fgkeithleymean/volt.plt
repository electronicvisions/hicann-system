set style data dots
#set title 'Voltage drop of floating gate current output vs time'
set terminal pdf size 14cm,10cm
set output "dvolt.pdf"
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
plot 'log.dat' using ($1/3600):($46-$47):($46+$47) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):46 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($42-$43):($42+$43) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):42 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($38-$39):($38+$39) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):38 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($34-$35):($34+$35) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):34 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($30-$31):($30+$31) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):30 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($26-$27):($26+$27) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):26 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($22-$23):($22+$23) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):22 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($18-$19):($18+$19) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):18 with lines lc rgb "black" notitle,  'log.dat' using ($1/3600):($14-$15):($14+$15) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):14 with lines lc rgb "black" notitle,  'log.dat' using ($1/3600):($10-$11):($10+$11) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):10 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($6-$7):($6+$7) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):6 with lines lc rgb "black" notitle, 'log.dat' using ($1/3600):($2-$3):($2+$3) with filledcurves  lc rgb "black" notitle, 'log.dat' using ($1/3600):2 with lines lc rgb "black" notitle 
set size 1,0.42
set origin 0,0
set ylabel 'voltage[V] ' offset 1,5
set yrange [0.53:0.55]
set ytics 0.005
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
