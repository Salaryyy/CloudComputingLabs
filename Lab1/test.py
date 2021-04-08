# Import package
# coding=utf-8
import numpy as np
import matplotlib.pyplot as plt

def index(x):
    return int(x)

# Assign filename to variable: file
file = 'result.dat'
file_ = 'result_.dat'

# Load file as array: digits
digits = np.loadtxt(file, delimiter=' ',usecols=(0,1,3,6),converters={0:index,6:index})
digits_ = np.loadtxt(file_, delimiter=' ',usecols=(0,5))
# print(digits)
tmp=digits[0][0]
aaa=digits[0][3]
tmp_=digits_[0][0]
aaa_=digits_[0][1]
num=0
sum=0.0
num_=0
sum_=0.0
dicta={}
dicta_={}
for di_ in digits_:
    sum_=sum_+di_[0]
    num_=num_+1
sum_=sum_/num_
#print(sum_)
for di in digits:
    if di[0]!=tmp:
        sum=sum/num
        dicta[tmp]=sum
        dicta_[tmp]=sum_/sum
        tmp=di[0]
        sum=di[1]
        num=1
    else:
        sum=sum+di[1]
        num=num+1
sum=sum/num
dicta[tmp]=sum
dicta_[tmp]=sum_/sum
#print(dicta)
#print("hello,world")

x1=list(dicta_.keys())
y1=list(dicta_.values())
x2=list(dicta.keys())
y2=list(dicta.values())
maxthread=int(tmp)
x = range(1,maxthread+1,1)
fig=plt.figure()
ax1=fig.add_subplot(111)
plt.rcParams['font.sans-serif'] = ['SimHei'] # 指定默认字体
plt.rcParams['axes.unicode_minus'] = False # 解决保存图像是负号'-'显示为方块的问题
ax1.plot(x2,y2,label="时间/s",color='red')
ax1.set_xlabel("线程数")
plt.legend(loc='upper left')
ax2=ax1.twinx()
ax2.plot(x1,y1,label="加速比")
plt.legend(loc='upper right')
#plt.xticks(rotation=30)#设置x轴上的刻度旋转角度
#plt.ylabel('时间')
plt.title('统计图')
plt.xticks(x)

#plt.legend()#需要配合这个才能显示图例标签
plt.savefig('./test2.jpg')