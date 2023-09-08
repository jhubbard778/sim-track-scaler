SOURCE=scale.c
NAME=$(basename $(SOURCE))
ARGS=track 0.5

all: $(NAME)

$(NAME): $(SOURCE)
	cc -std=gnu11 -Wall -Werror $(SOURCE) -o $(NAME) -g -lm

run: $(NAME)
	./$(NAME) $(ARGS)

.PHONY: clean test
clean:
	rm -rf $(NAME) $(NAME).dSYM

