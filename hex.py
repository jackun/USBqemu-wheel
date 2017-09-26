import sys

f=open(sys.argv[-1], 'rb')
b=f.read()
f.close()

with open(sys.argv[-1] + ".cpp", 'w') as w:
	c=0
	w.write("const unsigned char mjpg_frame[] = {\n")
	for i in b:
		w.write("0x%02x," % i)
		c+=1
		if c==12:
			w.write("\n")
			c=0
	w.write("\n};")
