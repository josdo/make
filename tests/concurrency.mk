first:
	for x in a b c; do sleep 1; echo first$$x; done

d1: first
	for x in a b c; do sleep 1; echo d1$$x; done
	
d2: first
	for x in a b c; do sleep 1.5; echo d2$$x; done
	
d3: first
	for x in a b c; do sleep 0.5; echo d3$$x; done

diamond: d1 d2 d3
	@echo diamond complete