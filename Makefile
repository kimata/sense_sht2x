CC_FLAGS := -Wall -std=c99

sense_sht2x:



%: %.c
	gcc $(CC_FLAGS) -o $@ $< 
