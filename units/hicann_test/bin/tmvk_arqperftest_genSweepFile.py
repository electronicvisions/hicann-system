import itertools as it
import random

parameters = {
    'num': [10000],
    'tags': ['1'], #first test... '11', '1111', '11111111', '100000000', '1100000000', '111100000000', '1111111100000000'], # does not work?!?! , '0xf0f0', '0xffff'],
    'hrx':  [2, 3, 4, 5, 10, 15, 20, 25, 40, 80],
    'htx':  [2, 3, 4, 5, 10, 15, 20, 25, 40, 80, 160],
    'frx':  [2, 3, 4, 5, 10, 15, 20, 25, 40, 80, 160],
    'ftx':  [2, 3, 4, 5, 10, 15, 20, 25, 40, 80, 160, 320],
    'arb':  [1, 2, 3, 4, 5, 8, 10] # 0 => hangs...?
}

varNames = sorted(parameters)

class hashabledict(dict):
    def __hash__(self):
        return hash(tuple(sorted(self.items())))

combinations = [hashabledict(zip(varNames, prod)) for prod in it.product(*(parameters[varName] for varName in varNames))]

already_done = []
with file("sweep_try1.out", 'r') as fd:
    while(True):
        raw = fd.readline().rstrip().split()
        if not raw:
            break
        if raw[0] != "MACHINEIT":
            continue
        t = hashabledict()
        t['num'], t['tags'], t['hrx'], t['htx'], t['frx'], t['ftx'], t['arb'] = int(raw[1]), raw[2], int(raw[3]), int(raw[5]), int(raw[7]), int(raw[8]), int(raw[9])
        already_done.append(t)

already_done = set(already_done)
combinations = set(combinations)
print 'already done', len(already_done)
print 'total number of combinations', len(combinations)
combinations = list(combinations.difference(already_done))
print 'combinations to be done', len(combinations)

shuffled_combos = combinations
random.shuffle(shuffled_combos)
with file("sweep_try1.dat", 'w') as fd:
    for c in shuffled_combos:
        print >>fd, c['num'], c['tags'], c['hrx'], c['hrx'], c['htx'], c['htx'], c['frx'], c['ftx'], c['arb']
