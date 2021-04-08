Python需要`numpy` `matplotlib` 包
`matplotlib` 使用 `SimHei` 字体，请自行下载

test.sh 自动化脚本，从单线程开始，到指定的线程数为止进行测试，对每个线程数运行三次，消除抖动，然后运行十次，得出结果输出到 result.dat 文件中。最后调用 test.py 程序计算平均值以及绘图。

test.py 读取 result.dat 文件中的数据，进行平均值的计算以及统计图的绘制。

test2.py 读取 result.dat 以及 result2.dat 文件中的数据，result2.dat 文件从其他设备运行上面的自动化脚本得出，需复制到同一目录下。

注：需要将原实验代码编译后，命名为 `sudoku_` 放入同一目录中，以计算加速比