# Makefile for answering questions about make behavior.

# Can variable names include spaces?
a = A A
$(a) = B B B
$(A A) = 3

spaces:
	@echo $(a)
	@echo $(A A)
	@echo $(B B B)

# Can the assignment operator be used in a variable name?
# A: yes only by variable substitution
equalsign==
E$(equalsign) = e
F==f

equals:
	@echo E  $(E)
	@echo E= $(E=)
	@echo F  $(F)
	@echo F= $(F=)

# Can colon be used in a variable, target, and dep name?
# A: yes in variable, nullifies target and dependency
colon=:
C = $(colon)
C1$(colon) = c1
colon:
	@echo C $C
	@echo C1: $(C1:)

# Does variable assignment preserve whitespace?
# A: no
WS =      w       s

whitespace:
	echo $(WS)

# Can variable 1 reference variable 2 in its name and in its value before variable 2 is defined?
# A: cannot ref in name, can ref in value (in recipe only!)
VR$(VR2) = 0
VR1 = $(VR2)
VR2 = 0

var-ref:
	@echo VR  $(VR)
	@echo VR0 $(VR0)
	@echo VR1 $(VR1)
	@echo VR2 $(VR2)

# Can variable 1 be referenced by a target, dependency, and recipe before its defined? 
# A: cannot ref in target or dep, can ref in recipe
rule-ref0:
	@echo RR  $(RR)

rule-ref1$(RR):
	@echo rule-ref1$(RR) was run

rule-ref2: $(RR)
	@echo rule-ref2 was run

RR = rr

rule-ref3$(RR):
	@echo rule-ref3$(RR) was run

rule-ref4: $(RR)
	@echo rule-ref4 was run

$(RR):
	@echo using $(RR) as a dependency


# NAME0$L = value0
# NAME1 = value1$L
# names:
# 	@echo NAME0   $(NAME0)
# 	@echo NAME0$L $(NAME0$L)
# 	@echo NAME1   $(NAME1)

# l:
# 	@echo using l as dependency
# rule0$L:
# 	@echo run rule0$L
# rule1:$L
# 	@echo run rule1
# L = l

# # Does a duplicate rule definition cause an error?
# # A: yes, latter definition works, but both dependencies are still run
# duplicate: var-ref
# 	@echo duplicate1
# duplicate: equals
# 	@echo duplicate2

# Can a rule and variable name be the same?
# A: yes
same = saaame
same:
	@echo same $(same)

# How distinguish variable def from rule def?
# A: colon first means rule def, but =var is a variable assignment that is invalid
# A: equals first means var def, and : allowed in var def
$(equalsign)diff2 = same
diff:
	@echo $(=diff2)
	@echo $(diff2)
diff1: $(=diff2)
	@echo diff1

# diff3: =hey
diff4=:hey
diff4:
	@echo $(diff4)

# Are variables recursively resolved when parsing a rule?
# A: yes
VVN = $(VV3)

VV$(VVN):
	@echo VVN $(VVN)

VV1 = last
VV2 = $(VV1)
RESOLVEa$(VVN) = resolvefirst
VV3 = $(VV2)
RESOLVEb$(VVN) = resolvelast

VVdep$(VVN):
	@echo VVdep$(VVN)

VV$(VVN): VVdep$(VVN)
	@echo $(VVN)


# Are variables recursively resolved when parsing a recipe?
# A: yes, just not for the pathological P4 case
P1 = $
P2a = (
P2b = )
P3 = got constructed
P4 = $(P1)$(P2a)P3$(P2b)
P5 = $(P3)
P6 = $@ $@ $@$@ $
partial:
	@echo partial test
	echo $(VVN)
	echo $(P4)
	echo $(P5)
	echo $(VVN)
	echo $(P6)@ and more! @ @@

# Are variables recursively resolved when parsing a variable name?
# A: yes, sequentially
resolve-name:
	echo $(RESOLVEalast)
	echo $(RESOLVEa)
	echo $(RESOLVEblast)
	echo $(RESOLVEb)

# Does a multi-target rule run more than once? Try for file and file-less
# A: yes
multi1 : multi2 multi3

multi2 multi3 :
	echo run prereq

# Does $@ expand to all targets of a rule, or just the one being run?
# A: only the one being run
auto-multi : auto-multi1 auto-multi2

auto-multi1 auto-multi2 :
	echo targets running are $@

# How are $ special cases substituted?
DOLLAR = hey
dollar:
	echo $ dollar

# What are invalid lines?
# A: tabbed variable definitions ignored (outside rule context)
#    but white spaced variable definitions accepted
# A: many no-ops (all white or tab spaces), then a tabbed line, is still in a rule context
	INV1 = inv1
INV2 = inv2
invalid1:
	echo $(INV1)
	echo $(INV2)

	echo end of invalid recipes

INV3 = :

invalid2:
	echo $(INV3)
			echo 			3 tabs

invalid3: hi = 1

    INV4 = inv4

invalid4:
	echo $(INV4)
	  echo valid?
	echo $(INV5)

# This throws  *** unterminated variable reference.  Stop.
# inv$(INV3 = is this invalid?

# This throws no error if unused.
# But only when used, e.g. when a recipe is called, or e.g. when used in a rule def,
# throws an  *** unterminated variable reference.  Stop.
# with this lineno
INV5 = $(INV3

# invalid5$(INV5):
# 	echo invalid 5

# This throws  *** unterminated variable reference.  Stop.
# invalid5$(INV3 :
# 	echo this works?

invalid6:
	echo $(INV5)

# When detect loop?
# A: at runtime
loop: loop1
loop1 : loop2
loop2 : loop1

LOOP1 = $(LOOP2)
LOOP2 = $(LOOP1)
LOOP3 = $(LOOP3)

loop3:
	echo $(LOOP3)
	echo $(LOOP1)
	echo $(LOOP2)

# loop4$(LOOP1):
# 	echo target name has variable loop

# loop3:
# 	echo repeat

# missing separator

# How handle duplicate prereqs?
# A: ignores duplicate
depend1:
	echo depend1
depend2:
	echo depend2
depend3: depend1 depend1
depend4: depend1
depend4: depend1

# How handle prereq that is not a target?
# A: error if no rule for a prereq
notarget1: notarget2

# How are special 1-char variables treated with parentheses?
# A: for automatic vars, they are treated the same.
#    for $($), it is treated as not a var, but the spec says to treat it as a var.
special-vars:
	echo $@
	echo $(@)
	echo $$
	echo $($)

# # How are tabbed non-recipes handled?
# notarecipe= # needed to ensure not continuing in the context of the prior rule
# 	not a recipe

# Can hashtag be assigned to a variable?
# A: no
hashtag = #
hash_target:
	echo $(hashtag)