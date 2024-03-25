# Error: no target specified. Note: make seems to allow this, but tinyMake
# should generate an error.
: prereq1 prereq2
	echo Output for unnamed target

all:
	echo "Output for 'all' target"
