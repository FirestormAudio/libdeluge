/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* A dlopened object that calls back into its host, the way an scsynth UGen
   calls the server's API. Proves both directions of the symbol wiring. */
#include <stdio.h>

/* Provided by the host app, resolved at dlopen time via --export-dynamic. */
extern int demo_host_version(void);

int demo_plugin_init(void)
{
	int v = demo_host_version();
	printf("plugin: host reports version %d\n", v);
	return v * 2;
}
