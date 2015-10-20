file = open('64kb.txt', 'w+')
max64 = 65536

msg = '%M devan '
file.write(msg)

for i in range (len(msg), max64 - 20):
    file.write('.')

j = 9
for i in range (max64 - 20, max64 - 10):
    file.write(str(j))
    j -= 1

j = 9
for i in range (max64 - 10, max64):
    file.write(str(j))
    j -= 1

j = 1
for i in range (max64, max64 + 9):
    file.write(str(j))
    j += 1

