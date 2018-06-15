import copy
data = genfromtxt('train', delimiter="\t") 


offset = data[0][0]

datareordered = copy.deepcopy(data)
for i in range(data.size/2):                                                                                                                      
	data[i][0] = data[i][0] - offset
	datareordered[i][0] = data[i][0] 
	if (data[i][1] == 0):
		datareordered[i][1] = -1
	elif (data[i][1] == 448):
		datareordered[i][1] = -2
	else:
		data[i][1] = int(data[i][1] -448)
		t = data[i][1] -1
		datareordered[i][1] = t%2 + int(t/8)*2 + int((t%8)/2)*8
figure("Rasterplot", figsize=(16,10))

plot(datareordered[:,0],datareordered[:,1],'|',mew=2.,ms=18)
axis([0,0.2,-2,32])

ylabel('Neuron')                                                                                                                          
xlabel('biological time[s]')
#figure(2, figsize=(16,10))

#plot(data[:,0],data[:,1],'|',mew=2.,ms=18)
#axis([0,0.5,-2,32])

