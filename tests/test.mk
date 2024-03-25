# Adapted from John Ousterhout.
# This file contains a bunch of targets that can be used to test
# various aspects of tinyMake. Invoke these targets from the parent
# directory (e.g. "./tinyMake -f tests/test.mk basic").
# Make sure "basic" is used as the default target if no target is
# specified on the command line.

basic target2 target3:

# Simple targets with no prereqs
basic:
	echo Output from basic target

# Simple target that actually creates a file. Invoke multiple times
# to make sure only the first time rebuilds.
tests/basic2:
	echo File contents > tests/basic2

# Tests dependencies; try modifying tests/deps* to see what rebuilds.
tests/deps: tests/deps1 tests/deps2
	echo "Contents of deps1:" > tests/deps
	cat tests/deps1 >> tests/deps
	echo "Contents of deps2:" >> tests/deps
	cat tests/deps2 >> tests/deps
	
tests/deps2: tests/deps3
	echo "New contents of deps3:" > tests/deps2
	cat tests/deps3 >> tests/deps2

tests/deps1:
	echo deps1 > tests/deps1

tests/deps3:
	echo deps3 > tests/deps3

# Test variable expansion
VAR1 = abc def
VAR2 = xxx$(VAR1)yyy
VAR3 = $(VAR2) $(VAR1)
VAR4 = $@
VAR5 = x$@$^$<y
$(VAR5) = 12345
X = xxx
var1:
	echo $(VAR1)
var2:
	echo $(VAR2)
var3:
	echo $(VAR3)
shortVarNames:
	echo 111 $X$$$(X)$Y 222
varDoesntExist:
	echo ---$(NO_SUCH_VAR) $Z---
autoVars: tests/deps1 tests/deps2 tests/deps3
	echo target: $@, first prereq: $<, all prereqs: $^
autoVarNested:
	echo $(VAR4)
trailingDollar:
	echo abc$
ignoreAutoVars:
	echo $(xy)

# Recursive variable expansion (illegal)
VAR6 = $(VAR6)
recursive:
	echo $(VAR6)

# Test variable expansion in the name of a variable
VAR_NAME = name1
$(VAR_NAME) = Value of name1
expandVarName:
	echo $(name1)
	
# Test variable expansion in prerequisites: 'make expPrereqs'

PREREQS = prereq1 prereq2
expPrereqs: $(PREREQS) prereq3
prereq1:
	echo building prereq1
prereq2:
	echo building prereq2
prereq3:
	echo building prereq3
	
# Test variable expansion in targets: 'make expTargets'
expTargets: expTarget1 expTarget2
TARGETS = expTarget1 expTarget2
$(TARGETS) : prereq1
	
# Make sure variable expansion in recipes doesn't happen until
# just before executing the recipe
VAR7 = Target is $@
expRecipes1:
	echo $(VAR7)
expRecipes2:
	echo $(VAR7)

# Tests failure of a recipe (error message must show file name and
# line number)
fail:
	exit 22
	echo This line should not appear

# Tests handling of variable syntax errors
BADVAR = $(abc
badvar:
	echo $(FOO
badvar2:
	echo $(BADVAR)

# Error: target has no prereqs or recipes.
nullTarget:

# Build this target with varying values of -j to test concurrent builds.
# Also try "./tinyMake -f test.test.mk -j 10 p1 p2 p3": all targets should
# build in parallel.
parallel: p1 p2 p3
	
p1:
	for x in a b c; do sleep 1; echo p1$$x; done
	
p2:
	for x in a b c; do sleep 1.5; echo p2$$x; done
	
p3:
	for x in a b c; do sleep 0.5; echo p3$$x; done
	
# Cleans up any modifications made during tests.
clean:
	rm -f tests/basic2 tests/deps*