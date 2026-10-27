set pagination off
set confirm off
set print thread-events on
set breakpoint pending on
set disable-randomization off
break /home/minh/embedded_asgn2/app/circular_buffer.c:62
commands
  silent
  printf "===== TASK 3 GDB: Processor reads shared buffer UNDER MUTEX =====\n"
  info threads
  thread apply all bt 8
  printf "===== BUFFER SNAPSHOT WHILE MUTEX HELD =====\n"
  print b->head
  print b->tail
  print b->current_size
  print b->total_produced
  print b->total_consumed
  print b->capacity
  printf "===== GDB SNAPSHOT COMPLETE =====\n"
  disable 1
  continue
end
run
