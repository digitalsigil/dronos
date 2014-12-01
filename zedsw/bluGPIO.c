#include <stdlib.h>
#include <strings.h>

struct gpio {
	char	*root;
}

struct	gpio *initGPIO(char *);
void	freeGPIO(struct gpio *);
void	setupGPIOpin(int, int);
void	writeGPIOpin(int, int);

struct gpio *
initGPIOpin(char *root)
{
	struct	gpio *rv;
	int	n;
	
	if ((rv = malloc(sizeof (struct gpio))) == NULL)
		goto error;
	
	n = strlen(root) + 1;
	if ((rv->root = malloc(n * sizeof (char))) == NULL)
		goto error;
	strcpy(rv->root, root, n);
	
	return rv;

error:
	if (rv == NULL)
		return NULL;

	if (rv->root != NULL)
		free(rv->root);
	free(rv);
	return NULL;
}

void
freeGPIO(struct gpio * g)
{
	free(g->root);
	free(g);
}

void
setupGPIOpin(int pin, int dir)
{
	
}
