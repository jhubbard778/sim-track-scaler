SOURCE=scale.c
NAME=$(basename $(SOURCE))

$(NAME): $(SOURCE)
	cc -std=gnu11 -Wall -Werror $(SOURCE) -o $(NAME) -g -lm

.PHONY: clean test
clean:
	rm -rf $(NAME) $(NAME).dSYM

