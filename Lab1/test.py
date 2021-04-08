# Import package
import numpy as np
import matplotlib.pyplot as plt


def index(x):
    return int(x)


# Assign filename to variable: file
file = 'result.dat'
 

# Load file as array: digits
digits = np.loadtxt(file, delimiter=' ',usecols=(0,1,3,6),converters={0:index,6:index})
# print(digits)
tmp=digits[0][0]
aaa=digits[0][3]
print(tmp)
num=0
sum=0.0
dicta={}
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
print(dicta)
#print("hello,world")

x2=list(dicta.keys())
y2=list(dicta.values())
plt.plot(x2,y2,label=str(aaa))
plt.xticks(rotation=30)#设置x轴上的刻度旋转角度
plt.xlabel('线程数')
plt.ylabel('时间')
plt.title('统计图')

plt.legend()#需要配合这个才能显示图例标签
plt.savefig('./test2.jpg')