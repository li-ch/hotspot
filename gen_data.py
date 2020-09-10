import random

filename = "./data/test03.txt"

time = 1000479359
n = 50
intervals = [5, 50, 100, 500, 1000]
with open(filename, 'w') as f:
    for i in range(n):
        interval = random.choice(intervals)
        addr = random.randint(2000000,5000000)
        for j in range(interval):
            time += 5
            hex_addr = hex(addr)
            addr += 4
            line = "{} R {}\n".format(time, addr)
            f.write(line)


