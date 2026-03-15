savedcmd_slab_stat.mod := printf '%s\n'   slab_stat.o | awk '!x[$$0]++ { print("./"$$0) }' > slab_stat.mod
