reset
set ylabel 'time(sec)'
set xlabel 'threads'
set style fill solid
set key left
set title 'perfomance comparison'
set term png enhanced font 'Verdana,10'
set output 'runtime.png'


plot [:][:] 'time.txt' using 2:xtic(1) with histogram title ' ', \
'' using ($0+0.2):($2+0.008):2 with labels title ' ', \
