import sys
import re

filename = 's3 dezvolt.kicad_sch'
with open(filename, 'r', encoding='utf-8') as f:
    data = f.read()

# Find all instances of ECRAN TFT or J3
matches = re.finditer(r'(?i).{0,100}ECRAN TFT.{0,100}', data)
for m in matches:
    print(m.group(0))
    
print("========")
# Find all labels to see if any correspond to J1 pins
labels = re.findall(r'\(label\s+"([^"]+)"', data)
print(set(labels))
