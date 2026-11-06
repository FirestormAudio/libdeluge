/* SPDX-License-Identifier: MIT OR Apache-2.0 */
/* dlopen_demo: dlopen a plugin from the app's private $ORIGIN prefix, call
   into it, and check that it called back into the app. Exits 0 on success.

   A static musl build cannot dlopen at all, which is why this app uses
   deluge_add_app(... LINKAGE DYNAMIC). */
#include <dlfcn.h>
#include <stdio.h>

int demo_host_version(void)
{
	return 21;
}

int main(void)
{
	void *h = dlopen("libdemoplugin.so.1", RTLD_NOW);
	if (!h) {
		fprintf(stderr, "dlopen failed: %s\n", dlerror());
		return 1;
	}

	int (*init)(void) = dlsym(h, "demo_plugin_init");
	if (!init) {
		fprintf(stderr, "dlsym failed: %s\n", dlerror());
		dlclose(h);
		return 1;
	}

	int result = init();
	printf("demo: plugin returned %d\n", result);
	dlclose(h);

	return result == 42 ? 0 : 1;
}
