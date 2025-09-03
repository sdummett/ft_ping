NAME      := ft_ping

SRC_DIR   := src
INC_DIR   := include
BLD_DIR   := build
OBJ_DIR   := $(BLD_DIR)/obj
BIN       := $(BLD_DIR)/$(NAME)

CC        := cc
CFLAGS    := -Wall -Wextra -Werror -I$(INC_DIR)
LDFLAGS   := -lm                       # (libs will be there)

SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRCS))


all: $(BIN)

$(BIN): $(OBJS) | $(BLD_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)
	sudo setcap cap_net_raw+ep $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD_DIR)  :
	@mkdir -p $@
$(OBJ_DIR)  :
	@mkdir -p $@

clean:
	$(RM) -r $(OBJ_DIR)

fclean: clean
	$(RM) -r $(BIN)

re: fclean all

.PHONY: all clean fclean re
