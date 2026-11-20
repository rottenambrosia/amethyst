# There's something 'bout you that now I cant remember
# It's the same damn thing that made my heart surrender
# I miss you on the train, I miss you in the mornings, I-
# Never know when to say goodbye (I think about you)

amethyst: amethyst.c
	$(CC) amethyst.c -o amethyst_editor.exe -Wall -Wextra -pedantic -std=c99
	$(CC) amethyst.c -o amethyst_editor -Wall -Wextra -pedantic -std=c99