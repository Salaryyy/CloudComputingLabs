# Import package
# coding=utf-8
import numpy as np
import matplotlib.pyplot as plt

def index(x):
    return int(x)

# Assign filename to variable: file
file = 'result.dat'
file2 = 'result2.dat'

# Load file as array: digits
digits = np.loadtxt(file, delimiter=' ',usecols=(0,1,3,6),converters={0:index,6:index})
digits2 = np.loadtxt(file2, delimiter=' ',usecols=(0,1,3,6),converters={0:index,6:index})
# print(digits)
tmp=digits[0][0]
aaa=digits[0][3]
tmp2=digits2[0][0]
aaa2=digits2[0][3]
num=0
sum=0.0
num2=0
sum2=0.0
dicta={}
dicta2={}

for di in digits:
    if di[0]!=tmp:
        sum=sum/num
        dicta[tmp]=sum
        tmp=di[0]
        sum=di[1]
        num=1
    else:
        sum=sum+di[1]
        num=num+1
sum=sum/num
dicta[tmp]=sum

for di in digits2:
    if di[0]!=tmp2:
        sum2=sum2/num2
        dicta2[tmp2]=sum2
        tmp2=di[0]
        sum2=di[1]
        num2=1
    else:
        sum2=sum2+di[1]
        num2=num2+1
sum2=sum2/num2
dicta2[tmp2]=sum2

#print(dicta)
#print("hello,world")

x2=list(dicta.keys())
y2=list(dicta.values())
x1=list(dicta2.keys())
y1=list(dicta2.values())
maxthread=int(tmp)
x = range(1,maxthread+1,1)
plt.rcParams['font.sans-serif'] = ['SimHei'] # 指定默认字体
plt.rcParams['axes.unicode_minus'] = False # 解决保存图像是负号'-'显示为方块的问题
plt.plot(x1,y1,label="ENV 1",color='blue')
plt.plot(x2,y2,label="ENV 2",color='red')
plt.xlabel("线程数")
#plt.xticks(rotation=30)#设置x轴上的刻度旋转角度
plt.ylabel('时间/s')
plt.title('运行时间对比')
plt.xticks(x)
plt.legend()#需要配合这个才能显示图例标签
plt.savefig('./test3.jpg')