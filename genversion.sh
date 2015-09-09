(
	echo 'extern const char app_version[];'
	echo 'extern const char app_date[];'
	echo 'extern const char app_diff_stat[];'
	echo 'extern const char app_diff_full[];'
) > version.h

(
	echo 'const char app_version[] = "'`git describe --tags --long`'";'
	echo 'const char app_date[] = "'`git log -n 1 --format=%ai`'";'

	echo 'const char app_diff_stat[] = ""'
	git diff --stat |sed 's/^/\"/; s/$/\\n"/'
	echo '"";'

	echo 'const char app_diff_full[] = ""'
	git diff -U0 |sed 's/\\/\\\\/g; s/\\n/\\\\n/g; s/\"/\\"/g; s/^/\"/; s/$/\\n"/'
	echo '"";'
) > version.c

