EXTRA_DIST = flexmalloc.sh count.sh

install-data-hook:
	mkdir -p $(bindir)
	cp $(srcdir)/flexmalloc.sh $(srcdir)/count.sh $(bindir)
	sed "s|@__sub_FLEXMALLOC_HOME__@|$(prefix)|g" -i $(bindir)/flexmalloc.sh
	sed "s|@__sub_FLEXMALLOC_HOME__@|$(prefix)|g" -i $(bindir)/count.sh
	chmod +x $(bindir)/flexmalloc.sh $(bindir)/count.sh
