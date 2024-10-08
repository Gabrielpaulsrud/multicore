main:
	gcc -std=gnu18 -o preflow preflow.c pthread_barrier.c -g -O3 -pthread
	time sh check-solution.sh ./preflow
	# @for i in {1..30}; do \
	# 	echo "Run $$i"; \
	# 	time sh check-solution.sh ./preflow || { echo "Failed on iteration $$i"; exit 1; }; \
	# done
	@echo PASS all tests
