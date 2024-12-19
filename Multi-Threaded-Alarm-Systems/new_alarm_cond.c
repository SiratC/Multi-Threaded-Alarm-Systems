/*
*  Team Members:
Moses Luna                     - 218840009
Jalp Panchal                   - 217956715
Ursanne Kengne                 - 218591560
Syeda Chowdhury                - 217010349
Franco Miguel Dela Cruz        - 216413742

*/

#include "errors.h"
#include <inttypes.h> // Include for PRIxPTR
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h> // Required for uintptr_t
#include <stdio.h>
#include <string.h>
#include <semaphore.h>

#define MAIN_THREAD_ID                                                         \
  1 // define id of main_thread to be 1 since a unique main thread would be
    // present throughout the process
#define PRINTING_INTERVAL 1
const bool DEBUGGER = false; //debugger is used to test and debug code. Change to true for debugging

// Alarm Status
#define ACTIVE 1
#define SUSPENDED 0
// Debugging macro to make debugging easier
#define DEBUG_PRINT(fmt, ...)                                                  \
  do {                                                                         \
    if (DEBUGGER)                                                              \
      fprintf(stderr, "DEBUGGER: " fmt "\n", __VA_ARGS__);                     \
  } while (0)


/*****SEMAPHORES IMPLEMENTATION ******/
sem_t mutx;  //semaphore mutex for alarm
sem_t writer_ac; // semaphore writer of alarm
int reader_counter; // counter to track number of reading processes for alarm operations


sem_t mutx_thread; // semaphore mutex for thread
sem_t writer_ac_thread; //semaphore writer of thread
int reader_counter_thread; // counter to track number of reading processes for thread operations

typedef struct alarm_t {
  int alarm_id;
  int seconds;
  char message[128];

  struct alarm_t *next;
  int status; // define true as 1 and false as 0
  time_t creation_time; // Time at which alarm is created
  time_t assign_time; // Assign time of alarm
  time_t expiration_time; // Time alarm has expired 
  int change_status;       // define true as 1 and false as 0 for any change
                           // performed.
  int change_group_status; // define true as 1 and false as 0 for any group
                           // change operation performed.
  int cancel_status; // define true as 1 and false as 0 for any cancel operation
                     // performed
  int end_status;
  int group_id;
  int time;

} alarm_t;

typedef struct thread_t {
  int thread_id;
  int alarm_counter;
  pthread_t thread;
  struct thread_t *next;
  // alarm_t *alarm;
  // alarm_t *alarm1;
  // alarm_t *alarm2;

  int group_id;

} thread_t;

/**
 * We create an alarm header with default values for each Data Type
 */
alarm_t alarm_header = {0, 0, "", NULL, 0, 0, 0, 0, 0};
alarm_t *ptr_ah = &alarm_header;

/**
 * Mutex of alarm list. Any function operating on the alarm list should have
 * this mutex locked. The calling function should later unlock the mutex once
 * operation is executed.
 */
pthread_mutex_t alarm_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * The condition of the alarm list which enables the thread to wait updates on
 * the alarm list.
 */
pthread_cond_t alarm_cond = PTHREAD_COND_INITIALIZER;

/**
 * Represents the header of the thread structure
 */
thread_t thread_header = {0, 0, 0, 0, 0};

/**
 * Mutex of thread list. Similarly, any thread modifying the list must first
 * lock this mutex. After an operation is performed on the list, the mutex must
 * be unlocked.
 */
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
Semaphore for reading alarm entry
*/
void reader_entry_handler(){
  sem_wait(&mutx);
  reader_counter++;
  if( reader_counter == 1){
    sem_wait(&writer_ac);
  }
  sem_post(&mutx);

}
/**
Semaphore for reading alarm exist
*/
void reader_exist_handler(){
  sem_wait(&mutx);
  reader_counter--;
  if(reader_counter == 0){
    sem_post(&writer_ac);
  }
  sem_post(&mutx);
}
/**
Semaphore for writing alarm entry
 */
void writer_entry_handler(){
  sem_wait(&writer_ac);
}
/**
Semaphore for writing alarm exist
 */
void writer_exist_handler(){
  sem_post(&writer_ac);
}
/**
init Semaphore for alarm
 */
void init_semps(){
  sem_init(&mutx, 0, 1);
  sem_init(&writer_ac, 0, 1);
}
/**
kill Semaphore for alarm
 */
void kill_sem(){
  sem_destroy(&mutx);
  sem_destroy(&writer_ac);
}




/**
 * Finds an alarm in the list using a specified ID
 *
 * Alarm list has to be locked by the caller of this method
 *
 * Loops through the linked list and returns pointer of alarm.
 *
 * If the specified ID is not found, return NULL.
 */
alarm_t *search_alarm_by_id(int id) {
  reader_entry_handler();
  DEBUG_PRINT("find_alarm_by_id: Searching for alarm with ID %d\n", id);
  alarm_t *alarm_node = alarm_header.next;

  // Loop through the list
  while (alarm_node != NULL) {
    // if ID is found, return pointer to it
    if (alarm_node->alarm_id == id) {
      DEBUG_PRINT("find_alarm_by_id: Alarm with ID %d found\n", id);
      return alarm_node;
    }
    // if ID is NOT found, go to next node
    else {
      alarm_node = alarm_node->next;
    }
  }
  // If the entire list was searched and specified ID was not
  // found, return NULL.
  DEBUG_PRINT("find_alarm_by_id: Alarm with ID %d not found\n", id);
  reader_exist_handler();
  return NULL;
}

/**
 * Inserts an alarm into the list of alarms.
 *
 * The alarm list mutex MUST BE LOCKED by the caller of this method.
 *
 * If an item in the list already exist with the given alarm's
 * alarm_id, this method returns NULL (and prints an error message to
 * the console). Otherwise, the alarm is added to the list and the
 * alarm is returned.
 */
alarm_t *insert_alarm_into_list(alarm_t *alarm) {
  //writer_entry_handler();
  DEBUG_PRINT("insert_alarm_into_list: Inserting alarm with ID %d\n",
              alarm->alarm_id);
  alarm_t *alarm_node = &alarm_header;
  alarm_t *next_alarm_node = alarm_header.next;

  // Find where to insert it by comparing alarm_id. The
  // list should always be sorted by alarm_id.
  while (next_alarm_node != NULL) {
    if (alarm->alarm_id == next_alarm_node->alarm_id &&
        alarm->change_status == 0 && alarm->change_group_status == 0) {
      /*
       * Not possible because 2 alarms cannot have the same_id
       * Slight exception for an alarm whose status has been changed
       * In this case, unlock mutex and try again
       */
      printf("Alarm with same ID exists\n");
       //writer_exist_handler(); 
      return NULL;
    } else if (alarm->alarm_id < next_alarm_node->alarm_id) {
      // Insert before next_alarm_node
      alarm_node->next = alarm;
      alarm->next = next_alarm_node;
      DEBUG_PRINT("Insert_alarm_into_list: Alarm with ID %d inserted\n",
                  alarm->alarm_id);
      return alarm;
    } else {
      alarm_node = next_alarm_node;
      next_alarm_node = next_alarm_node->next;
    }
  }

  // Insert at the end of the list
  alarm_node->next = alarm;
  alarm->next = NULL;
  DEBUG_PRINT("Insert_alarm_into_list: Alarm with ID %d added to end of list\n",
              alarm->alarm_id);
 // writer_exist_handler();            
  return alarm;
}

/*
 * Deletes an alarm with changed status (old alarm) from the list of alarms.
 *
 * It is important that the ALARM MUTEX is LOCKED by the caller of this method.
 *
 * Loops through the linked list of alarms and locate thread to be removed
 * Once thread is located, links are updated to remove thread
 */

alarm_t *remove_duplicate_alarm_from_list(int id) {
  //writer_entry_handler();
  DEBUG_PRINT("remove_alarm with_changed_status_from_list: Removing "
              "alarm_changed with ID %d\n",
              id);
  alarm_t *alarm_node = alarm_header.next;
  alarm_t *alarm_prev = &alarm_header;

  // Keeps on searching the list until it finds the correct ID
  while (alarm_node != NULL) {
    if (alarm_node->alarm_id == id) {
      if (alarm_node->change_status == 1 &&
          alarm_node->change_group_status == 1) {
        // set debugger here

        DEBUG_PRINT("remove_alarm with_changed_status_from_list: Alarm_changed "
                    "with ID %d removed\n",
                    id);
        alarm_prev->next = alarm_node->next;
        break; // Exit loop since ID has been found with change status =1 has
               // been found
      }
    }
    // If the ID is not found, move one node ahead of the current one
    alarm_node = alarm_node->next;
    alarm_prev = alarm_prev->next;
  }
  DEBUG_PRINT("remove_alarm with_changed_status_from_list: Alarm_changed with "
              "ID %d not found\n",
              id);
  //writer_exist_handler();            
  return alarm_node;
}
/**
 * Deletes an alarm from the list of alarms.
 *
 * It is important that the ALARM MUTEX be LOCKED by the caller of this method.
 *
 * Loops through the linked list of alarms and locate thread to be removed
 * Once thread is located, links are updated to remove thread
 */

alarm_t *remove_alarm_from_list(int alarm_id) {
 // writer_entry_handler();
  DEBUG_PRINT("remove_alarm_from_list: Removing alarm with ID %d\n", alarm_id);
  alarm_t *alarm_node = alarm_header.next;
  alarm_t *alarm_prev = &alarm_header;

  // Loops through until right ID is found
  while (alarm_node != NULL) {
    if (alarm_node->alarm_id == alarm_id) {
      // set debugger here
      alarm_prev->next = alarm_node->next;
      DEBUG_PRINT("remove_alarm_from_list: Alarm with ID %d removed\n",
                  alarm_id);
      break; // break out since ID has been located
    }
    // If the ID is not found, move one node ahead of the current one

    alarm_node = alarm_node->next;
    alarm_prev = alarm_prev->next;
  }
  DEBUG_PRINT("remove_alarm_from_list: Alarm with ID %d not found\n", alarm_id);
  //writer_exist_handler();
  return alarm_node;
}

/**
 * Adds a thread to the thread list.
 *
 * The caller of this function must have the thread list mutex locked when
 * calling this function.
 *
 * The thread is added and linked to the most recent added thread, and is
 * adjusted to the recent thread
 */
void add_to_thread_list(thread_t *thread) {
  
  DEBUG_PRINT("add_to_thread_list: Adding thread with ID %d\n",
              thread->thread_id);
  thread_t *current_thread = &thread_header;

  while (current_thread != NULL) {
    if (current_thread->next == NULL) {

      current_thread->next = thread;
      DEBUG_PRINT("add_to_thread_list: Thread with ID %d added to list\n",
                  thread->thread_id);
      break;
    }
    current_thread = current_thread->next;
  }
  
}

/**
 * Deletes a thread from the thread list.
 *
 * The caller of this function must have the thread list mutex locked when
 * calling this function to avoid synchronization issues
 *
 * It loops through the thread link of threads in list, and finds thread to be
 * removed Once thread is found, links are simply updated.
 */
thread_t *remove_from_thread_list(thread_t *thread) {
 
  DEBUG_PRINT("remove_from_thread_list: Removing thread with ID %d\n",
              thread->thread_id);

  thread_t *thread_node = thread_header.next;
  thread_t *thread_prev = &thread_header;

  // loops through list until the exact thread is found
  while (thread_prev != NULL) {

    if (thread_node == thread) {

      thread_prev->next = thread_node->next; // Remove thread from list.
      DEBUG_PRINT("remove_from_thread_list: Thread with ID %d removed\n",
                  thread->thread_id);
      break; // break out  loop.
    }
    // If the ID is not found, move one node ahead
    thread_node = thread_node->next;
    thread_prev = thread_prev->next;
  }
  
  return thread_node;
}

/*
 * Checks if alarm_type exists in a thread.
 *This function would be used to determine if an additional
 *display thread would be printed or a new display thread here
 */
int alarm_groupID_exist_in_thread(alarm_t *alarm) {
  
  DEBUG_PRINT("thread_alarm_group_existence_check: Checking if an alarm having "
              "the same group ID as the present alarm already exists "
              "already exists in a thread %s",
              "Entry\n");
  thread_t *current_thread = thread_header.next;

  while (current_thread != NULL) {
    if (current_thread->group_id == alarm->group_id) {
      DEBUG_PRINT(
          "thread_alarm_group_existence_check: Thread with ID %d has an "
          "alarm with same group ID as that of the added alarm exists \n",
          current_thread->thread_id);
      // set debugger in if statement
      //  This means a thread exists for an alarm group.
      return 1; // true, alarm with group ID as added alarm exists in thread
    }

    current_thread = current_thread->next;
  }

  return 0; // false otherwise
}



/**
 * DISPLAY THREAD
 * * * * * * * * *
 *
 * This is the function containing both the alarm thread and display threads. It
 * will loop every 5 seconds, waiting on a condition variable. When the wait
 * times out, it prints the alarms that it keeps, unless an alarm expired, in
 * which case the alarm will be deleted.
 *
 * The main thread communicates to the display threads via a series of statuses
 * which are updated in an alarm each time an operation such as Start_Alarm,
 * Change_Alarm and Cancel_Alarm. It does that with the help of a
 * condition variable which broadcasts to the display list after each operation.
 * When the main thread needs an operation to be handled by a
 * display thread, it modifies the appriopriate status of an alarm for
 * a display thread to handle it properly, as well as broadcasting a condition
 * variable
 *
 * If a display thread needs to handle an operation as requested by the main
 * thread, it will perform actions to handle it via alarm's and reset alarms'
 * statuses once it is finished. Since a display thread handles a unique alarm,
 * we are sure that other alarms would not be impacted by a change of a single,
 * or a couple of alarms
 *
 * Once a display thread has no alarms left (either because they expired or were
 * cancelled) it will delete and free its entry from the thread list, after it
 * returns from its function. (which will allow the thread to be recycled by the
 * operating system).
 */
void *alarm_thread(void *arg) {
  //reader_entry_handler_th();
  thread_t *thread = ((thread_t *)arg); // The thread parameter passed to this
                                        // thread.
                                        // int alarm_counter=0;

  int status; // Vaariable to hold the status
              // returns via type condition
              // variable waits.

  time_t now; // Variable to hold the current time.

  struct timespec timeout; // Variable for determining timeouts
                           // for conditional waits
   int thread_null = 0; //signal that thread is no longer useful
  // set debugger here
  DEBUG_PRINT("Alarm_thread: Creating thread %d\n", thread->thread_id);

  // /*
  //  * Lock the mutex for thread to access alarm list.
  //  */
   pthread_mutex_lock(&alarm_mutex);

  while (1) {

    // Set debugger here
    alarm_t *current_alarm = alarm_header.next;
    while (current_alarm != NULL) {
      if (thread->group_id == current_alarm->group_id) {
        /*
         * Get current time (in seconds from UNIX epoch).
         */
        now = time(NULL);

        DEBUG_PRINT("Alarm_thread: Calculating Timeout for alarm(%d) %s",
                    current_alarm->alarm_id, "Started");

        if (current_alarm->expiration_time - now < 1)
          timeout.tv_sec = current_alarm->expiration_time;

        else
          timeout.tv_sec = now + PRINTING_INTERVAL;

        /*
         * Add 10 milliseconds (10,000,000 nanoseconds) to the timeout to make
         * sure the expiry is reached. Failure to do this results in a bug where
         * the alarm prints out many successive times before expiring.
         */
        timeout.tv_nsec = 10000000;

        DEBUG_PRINT("Alarm_thread: Calculating out  Timeout %s", "Ended");
        /*
         * Wait for condition variable to be broadcast. When we get a
         * broadcast, this thread will wake up and will have the mutex
         * locked.
         *
         * Since this is a timed wait, we may also be woken up by the
         * time expiring. In this case, the status returned will be
         * ETIMEDOUT.
         */
        status = pthread_cond_timedwait(&alarm_cond, &alarm_mutex, &timeout);

        /*
         * In this case, the last second of the alarm timed out, so we must print the
         * alarm.
         */
        if (status == ETIMEDOUT) {
          if (current_alarm->expiration_time <= time(NULL) &&
              current_alarm->status == 1) { // true

            printf("Alarm(%d) Printed by Display Alarm Thread(%d) at %ld: "
                   "Group(%d) %d %s\n",
                   current_alarm->alarm_id, thread->group_id, time(NULL),
                   thread->group_id, current_alarm->seconds,
                   current_alarm->message);

            current_alarm->expiration_time =
                time(NULL) +
                current_alarm->seconds; // reset the alarm expiration time

            /* END OF REMOVAL THREAD FUNCTION */

          }

          else if (current_alarm != NULL && current_alarm->status == 1) {

            /*
             * If the message for alarm has been recently changed, print that
             * the new display thread is starting to print the new message.
             */
            if (current_alarm->change_status == 1) {

              if (current_alarm->change_group_status == 1) {
                // Means Group of alarm has been changed.
                printf("Display Thread (%d) Has Stopped Printing Message of "
                       "Alarm(%d) at %ld: Changed Group(%d) %d %s\n",
                       thread->thread_id, current_alarm->alarm_id, time(NULL),
                       thread->group_id, current_alarm->seconds,
                       current_alarm->message);

                DEBUG_PRINT(
                    "Alarm (%d) Changed and discarded by  Display Thread %d at "
                    "%ld: Group(%d) %d %s\n",
                    current_alarm->alarm_id, thread->thread_id, time(NULL),
                    thread->group_id,
                    current_alarm
                        ->seconds, // time denotes the current printing time.
                    current_alarm->message);

                // We also check if it is last alarm in thread, in which case we
                // free the
                // thread while removing the alarm
                /***  REMOVAL THREAD FUNCTION  **/

                int counter =
                    thread->alarm_counter; // checks the number of alarms
                if (counter - 1 == 0) {

                  printf("No More Alarms in Group(%d) Alarm Removal Thread Has "
                         "Removed Display Thread(%d) at %ld: Group(%d) %d %s\n",
                         thread->group_id, thread->thread_id, time(NULL),
                         thread->group_id, current_alarm->seconds,
                         current_alarm->message);
                  // debugger
                  DEBUG_PRINT("Display_Thread_Terminated (%d) at %ld with "
                              "alarm (%d) for Group(%d): ",
                              thread->thread_id, time(NULL), thread->group_id,
                              thread->group_id);

                  /*
   //              * Lock thread list mutex, remove thread from list, free
   thread
   //              * (because it was malloced by the main thread), unlock thread
   list
   //              * mutex, and break so that we exit the main loop and this
   thread
   //              * can be destroyed.
   //              */
                 
                  remove_duplicate_alarm_from_list(current_alarm->alarm_id);
                  free(current_alarm);
                  thread_null = 1; //set thread_null to 1 to show that the thread needs to terminate immediately.
                  break;

                }

                else {
                  /*
                   *There is atleast an alarm remaining in thread, in which case
                   *we remove current alarm and make the thread to live while
                   *continuing to run the other alarms
                   */

                  DEBUG_PRINT("Display_thread(%d): Removing Alarm with ID %d",
                              thread->thread_id, current_alarm->alarm_id);
                  int storeID = current_alarm->alarm_id;
                  remove_duplicate_alarm_from_list(current_alarm->alarm_id);
                  free(current_alarm);
                  current_alarm = NULL;
                  pthread_mutex_lock(&thread_mutex);
                  thread->alarm_counter--;
                  pthread_mutex_unlock(&thread_mutex);

                  DEBUG_PRINT(
                      "Display_thread(%ld): Removal of Alarm with ID %d "
                      "succeeded ",
                      thread->thread, storeID);
                }

              } // end of change_group_check status

              else {

                // Implies alarm group did not change, then we print the New
                // changed alarm, and re-initialize the changed alarm to zero.

                printf("Display Thread (%d) Starts to Print Changed Message "
                       "Alarm(%d) at %ld: Group(%d) %d %s\n",
                       thread->thread_id, current_alarm->alarm_id, time(NULL),
                       thread->group_id, current_alarm->seconds,
                       current_alarm->message);

                DEBUG_PRINT(
                    "Alarm (%d) Changed with same Display Thread %d at "
                    "%ld: Group(%d) %d %s\n",
                    current_alarm->alarm_id, thread->thread_id, time(NULL),
                    thread->group_id,
                    current_alarm
                        ->seconds, // time denotes the current printing time.
                    current_alarm->message);

                current_alarm->change_status = 0;
              }
            } //end of change_status

            if (current_alarm != NULL && current_alarm->cancel_status == 1) {

              printf("Display Thread (%d) Has Stopped Printing Message of "
                     "Alarm(%d) at %ld: Group(%d) %d %s\n",
                     thread->thread_id, current_alarm->alarm_id, time(NULL),
                     thread->group_id, current_alarm->seconds,
                     current_alarm->message);

              // We also check if it is last alarm in thread, in which case we
              // free the
              // thread while removing the alarm
              /***  REMOVAL THREAD FUNCTION  **/

              int counter =
                  thread->alarm_counter; // checks the number of alarms
              if (counter - 1 == 0) {

                printf("No More Alarms in Group(%d) Alarm Removal Thread Has "
                       "Removed Display Thread(%d) at %ld: Group(%d) %d %s\n",
                       thread->group_id, thread->thread_id, time(NULL),
                       thread->group_id, current_alarm->seconds,
                       current_alarm->message);
                // debugger
                DEBUG_PRINT("Display_Thread_Terminated (%d) at %ld with alarm "
                            "(%d) for Group(%d): ",
                            thread->thread_id, time(NULL), thread->group_id,
                            thread->group_id);

                            thread_null = 1;

                /*
 //              * Lock thread list mutex, remove thread from list, free thread
 //              * (because it was malloced by the main thread), unlock thread
 list
 //              * mutex, and break so that we exit the main loop and this
 thread
 //              * can be destroyed.
 //              */

                remove_alarm_from_list(current_alarm->alarm_id);
                free(current_alarm);
                break;

              }

              else {
                /*
                 *There is atleast an alarm remaining in thread, in which case
                 *we remove current alarms and make the thread to live while
                 *continuing to run the other alarms
                 */

                DEBUG_PRINT("Display_thread(%d): Removing Alarm with ID %d",
                            thread->thread_id, current_alarm->alarm_id);
                int storeID = current_alarm->alarm_id;
                remove_alarm_from_list(current_alarm->alarm_id);
                free(current_alarm);
                current_alarm = NULL;
                pthread_mutex_lock(&thread_mutex);
                thread->alarm_counter--;
                pthread_mutex_unlock(&thread_mutex);

                DEBUG_PRINT("Display_thread(%d): Removal of Alarm with ID %d "
                            "succeeded ",
                            thread->thread_id, storeID);
              }

            } // end of cancel_status
          }
        }
      }
      current_alarm = current_alarm->next;
    }
    
    //END OF INTERNAL WHILE LOOP
    DEBUG_PRINT("%s", "Loop continues after printing");

  /*
   * UnLock the mutex for thread to free alarm list.
   */
 //pthread_mutex_unlock(&alarm_mutex);
 if(thread_null == 1){

   /*
 //              * Lock thread list mutex, remove thread from list, free thread
 //              * (because it was malloced by the main thread), unlock thread
 list
 //              * mutex, and break so that we exit the main loop and this
 thread
 //              * can be destroyed.
 //              */

 pthread_mutex_lock(&thread_mutex);
                
  // pthread_mutex_unlock(&alarm_mutex);
  remove_from_thread_list(thread);
  free(thread);
  pthread_mutex_unlock(&thread_mutex);
  break;
 }
 

   else
    continue;
  }

  /*
   * Unlock alarm list mutex after performing an operation on it.
   */
  //reader_exist_handler_th();  
  pthread_mutex_unlock(&alarm_mutex);
}

/**
 * This function loops through the thread list to print out
 * display threads along with their alarm numbers and contents
 *
 */
void view_alarms() {

  // int status = pthread_mutex_lock(&thread_mutex);
  // if (status != 0) {
  //   err_abort(status, "Lock mutex");
  // }
  DEBUG_PRINT("view_alarms starting at %ld:\n", time(NULL));
  thread_t *current_thread = thread_header.next;
  time_t view_time = time(NULL); // View time
  int thread_count = 1;
  char c ='a';

if(current_thread == NULL){
printf("There are no alarms to be viewed\n");
return;
  }
printf("view_alarms starting at %ld:%ld\n", time(NULL), time(NULL)); //starting to view_Alarms

  while(current_thread != NULL) {

    DEBUG_PRINT("view_alarms at %ld:\n", view_time);
      c = 'a';
    int counter_alarm_thread = 0;

      

      printf("%d. Display Thread %d Assigned:\n", thread_count,
             current_thread->thread_id);
      
      alarm_t *current_alarm = alarm_header.next;
      while(current_alarm != NULL) {
 
        if(current_thread->group_id == current_alarm->group_id){
           DEBUG_PRINT("Display Thread (%d), Alarm(%d) is printed out at %ld:\n",
                    current_thread->thread_id,
                    current_alarm->alarm_id, view_time);
           
           counter_alarm_thread++;
                  
       if(current_alarm->status == 1) //Means an alarm is active
        printf("\t%d%c. Alarm(%d): Created at %ld Assigned at %ld %d %s Status %s\n", thread_count, (char)c++,
               current_alarm->alarm_id,
               current_alarm->creation_time, current_alarm->assign_time,
               current_alarm->seconds,
               current_alarm->message,
               "active");
        
        else 
          
          printf("\t%d%c. Alarm(%d): Created at %ld Assigned at %ld %d %s Status %s\n", thread_count, (char)c++,
               current_alarm->alarm_id,
               current_alarm->creation_time, current_alarm->assign_time,
               current_alarm->seconds,
               current_alarm->message,
               "suspended"); //means alarm is suspended

               if(counter_alarm_thread == current_thread->alarm_counter)
                break; //Means we have found all alarms binded to the current thread, in this case, we move to the next thread
                //to save some execution time.

        }
        current_alarm = current_alarm->next; //move to the next alarm
        
      }
      
     
      current_thread = current_thread->next; 
      thread_count++;

      DEBUG_PRINT("While loop ends at %ld\n", time(NULL));
    
  } 
  
  

  // status = pthread_mutex_unlock(&thread_mutex);
  // if (status != 0) {
  //   err_abort(status, "Unlock mutex");
  // }

}
/*
Function used to Cancel the Alarm promptly
*/
void cancel_alarm_by_id(int alarm_id) {
  //writer_entry_handler();

  // lock the alarm list to cancel alarm properly
  int status = pthread_mutex_lock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Lock mutex");
  }

  // Get the alarm ID to be terminated
  alarm_t *current = search_alarm_by_id(alarm_id);
  // If no alarm was found with the given id

  if (current == NULL) {

    // if(DEBUGGER){
    // //     printf("DEBUGGER: Alarm to be cancelled does not exist in alarm
    // list\n");

    // // }
    printf("There are no alarms with id: %d present in the alarm_list\n",
           alarm_id);

    // If the above is true, unlock the mutex and stop processing the function
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0) {
      err_abort(status, "Unlock mutex");
    }
    return;
  }

  current->cancel_status = 1;
  // alarm_t *prev = NULL;
  time_t cancel_time = time(NULL);

  printf("Alarm(%d) Cancelled at %ld: %d %s\n", current->alarm_id, cancel_time,
         current->seconds, current->message);

  status = pthread_mutex_unlock(&alarm_mutex);
  if (status != 0) {
    err_abort(status, "Unlock mutex");
  }
 // writer_exist_handler();
}

/*
***
Suspend Alarm Function. Suspends Alarm upon User Request
*/

void suspend_alarm_by_id(int alarm_id) {
  //writer_entry_handler();
  DEBUG_PRINT("%s",
              "Alarm list mutex is locked by suspend_alarm_by_id Function\n");
  // Lock the alarm list to suspend alarm properly
  int status = pthread_mutex_lock(&alarm_mutex);

  if (status != 0) {
    DEBUG_PRINT("%s",
                "Alarm mutex lock failed for suspend_alarm_by_id Function\n");
    err_abort(status, "Lock mutex");
  }

  DEBUG_PRINT("Alarm(%d) is being searched in Alarm List\n", alarm_id);
  // Get the alarm ID to be suspended
  alarm_t *current = search_alarm_by_id(alarm_id);

  // Print error message if not alarm_id not found
  if (current == NULL) {
    DEBUG_PRINT("There are no alarms with id: %d present in the alarm list\n",
                alarm_id);
    printf("There are no alarms with id: %d present in the alarm list\n",
           alarm_id);

    // Unlock mutex once the error message above is printed
    DEBUG_PRINT(
        "%s",
        "Alarm list mutex is unlocked by suspend_alarm_by_id Function, and "
        "function returns without finding an alarm with the given ID\n");
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0) {
      err_abort(status, "Unlock mutex");
    }
    return;
  }

  // checking if alarm is already suspended
  if (current->status == SUSPENDED) {
    DEBUG_PRINT("Alarm(%d) is already suspended\n", current->alarm_id);
    // printf("Alarm(%d) is already suspended\n", current->alarm_id);
    return;
  }

  // Alarm status is now changed to suspended
  current->status = SUSPENDED;

  // Time the alarm was suspended
  time_t suspend_time = time(NULL);

  printf("Alarm(%d) Suspended at %ld:%d %s\n", current->alarm_id,
         suspend_time, current->seconds, current->message);

  // Letting other waiting threads (if any) know that the alarm status has now
  // changed
  pthread_cond_broadcast(&alarm_cond);

  //  DEBUG_PRINT("%s", "Alarm list mutex is unlocked by suspend_alarm_by_id"
  //  "Function, and function returns properly\n");

  status = pthread_mutex_unlock(&alarm_mutex);

  if (status != 0) {
    err_abort(status, "Unlock mutex");
  } else

    DEBUG_PRINT("%s", "Alarm list mutex is unlocked by suspend_alarm_by_id "
                      "Function, and function returns properly\n");
  //writer_exist_handler();                    
}

/**
 * Reactivate_Alarm Function
 *Reactivates alarm upon user Request
 *
 */
void reactivate_alarm_by_id(int alarm_id) {
//writer_entry_handler();
  DEBUG_PRINT(
      "%s", "Alarm list mutex is locked by reactivate_alarm_by_id Function\n");
  // Lock the alarm list to activate alarm properly

  int status = pthread_mutex_lock(&alarm_mutex);

  if (status != 0) {
    DEBUG_PRINT(
        "%s",
        "Alarm list mutex is unlocked by reactivate_alarm_by_id Function, and "
        "function returns without finding an alarm with the given ID\n");
    status = pthread_mutex_unlock(&alarm_mutex);
    err_abort(status, "Lock mutex");
  }

  // Get the alarm ID to be activated
  alarm_t *current = search_alarm_by_id(alarm_id);

  // Print error message if alarm_id is not found
  if (current == NULL) {
    printf("There are no alarms with id: %d present in the alarm list\n",
           alarm_id);

    // Unlock mutex once the error message above is printed
    status = pthread_mutex_unlock(&alarm_mutex);
    if (status != 0) {
      err_abort(status, "Unlock mutex");
    }
    return;
  }

  if (current->status == ACTIVE) {
    DEBUG_PRINT("Alarm(%d) is already active\n", current->alarm_id);
    // printf("Alarm(%d) is already active\n", current->alarm_id);
    return;
  }

  // Alarm status is now changed to ACTIVE
  current->status = ACTIVE;

  // Time the alarm was reactivated
  time_t reactivate_time = time(NULL);

  printf("Alarm(%d) Reactivated at %ld:%d %s\n", current->alarm_id,
         reactivate_time, current->seconds,
         current->message);

  // Letting other waiting threads (if any) know that the alarm status has now
  // changed
  pthread_cond_broadcast(&alarm_cond);

  status = pthread_mutex_unlock(&alarm_mutex);

  if (status != 0) {
    err_abort(status, "Unlock mutex");
  }
  DEBUG_PRINT("%s", "Alarm list mutex is unlocked by reactivate_alarm_by_id "
                    "Function, and function returns properly\n");
  //writer_exist_handler();                  
}

/**
 * MAIN THREAD
 * * * * * * *
 *
 * This is the function for the main thread (and the entry point to the
 * program). It reads commands from user input, parses the command, and performs
 * actions relating to the command.
 *
 * Commands will add, modify, and delete alarms. Each alarm is malloced by this
 * thread. Since each display thread can only hold a maximum of two alarms, this
 * thread is responsible for creating new display threads.
 *
 * When handling commands, the main thread will manipulate the alarm list and
 * create events and broadcast them to the display threads to be handled.
 * Display threads will wait on a condition variable, allowing the main thread
 * to wake up the display threads by broadcasting the condition variable.
 */

int main(int argc, char *argv[]) {

  int status;
  char line[128]; // buffer array for input insertion

  int alarm_id;
  char alarmR[15];
  pthread_t thread;
  int tA = 0;

  alarm_t *alarm; // Pointer for newly created alarms.

  thread_t *next_thread; // Pointer for newly created threads.

  int thread_id_counter = 0; // Counter for thread IDs. This will be
                             // incremented every time a thread is created so
                             // that each thread has a unique ID.

  // DEBUG_PRINT_START_MESSAGE();
  init_semps();
  
  

  while (1) {
    
    if (DEBUGGER)
      DEBUG_PRINT("main_thread: Debugger State: %s", "TRUE\n");

    printf("alarm> ");
    if (fgets(line, sizeof(line), stdin) == NULL) {
      // printf("fgets failed");
      exit(0);
    }

    if (strlen(line) <= 1) {
      // printf("strlen failed");
      continue;
    }

    // trim useless lines
    line[strcspn(line, "\n")] = '\0';
    DEBUG_PRINT("main_thread: Input State: %s", "Trim useless lines\n");

    // Parse the command type first.
    if (sscanf(line, "%18s", alarmR) != 1) {
      fprintf(stderr, "Bad command format\n");
      DEBUG_PRINT("main_thread: Input State: %s", "Bad command\n");
      continue;
    }

    // Handle "View_Alarms" directly, as it doesn't require further parsing.
    if (strncmp(alarmR, "View_Alarms", 12) == 0) {
      DEBUG_PRINT("main_thread: Input State: %s", "View_Alarm event\n");

      // View alarms function is called
      view_alarms();
      continue;
    }

    // Handle Reactivate_Alarms
    if (strncmp(alarmR, "Reactivate_Alarm", 16) == 0) {
        //invoke reactivate_alarm function
      reactivate_alarm_by_id(alarm->alarm_id);
      continue;
    }

    // Handle Suspend_Alarms
    if (strncmp(alarmR, "Suspend_Alarm", 13) == 0) {
     
      //invoke suspend alarm function ID
      suspend_alarm_by_id(alarm->alarm_id);
      continue;
    }

    // Handle "Cancel_Alarm" with minimal parsing

    if (strncmp(alarmR, "Cancel_Alarm", 12) == 0) {

      if (sscanf(line, "%*[^0123456789]%d", &alarm_id) == 1) {

        DEBUG_PRINT(
            "main_thread: Input State: Cancel Alarm: ID=%d, GroupID=%d, "
            "Time=%d, Message=%s\n",
            alarm->alarm_id, alarm->group_id, alarm->seconds, alarm->message);

        cancel_alarm_by_id(alarm_id);
        // Code to cancel the alarm with the given ID would go here.
      }

      else {
        fprintf(stderr, "Bad command format for Cancel_Alarm\n");
        DEBUG_PRINT("main_thread: Input State: %s", "Bad command\n");
      }
      continue;
    }

    // For "Start_Alarm" and "Change_Alarm", parse the full details.
    alarm = (alarm_t *)malloc(sizeof(alarm_t));
    DEBUG_PRINT("main_thread: Input State: %s", "Alarm malloc event\n");

    if (alarm == NULL)
      errno_abort("Allocate alarm");

    if (sscanf(line, "%15[^(](%d): Group(%d) %d %64[^\n]", alarmR,
               &alarm->alarm_id, &alarm->group_id, &alarm->seconds,
               alarm->message) < 5) {
      fprintf(stderr, "Bad command format for %s\n", alarmR);
      free(alarm);
      continue;
    }

    // Handle "Start_Alarm"
    if (strncmp(alarmR, "Start_Alarm", 11) == 0) {
      DEBUG_PRINT("main_thread: Input State: Start Alarm: ID=%d, GroupID=%d, "
                  "Time=%d, Message=%s\n",
                  alarm->alarm_id, alarm->group_id, alarm->seconds,
                  alarm->message);

      // lock the alarm mutex for alarm list modification and insertion into
      // list
      status = pthread_mutex_lock(&alarm_mutex);
      if (status != 0)
        err_abort(status, "Lock mutex");

      /*
       * Fill in data for alarm.
       */

      DEBUG_PRINT("main_thread: Alarm State: %s", "Alarm Data filling\n");
      alarm->status = 1; // true
      alarm->creation_time = time(NULL);
      alarm->assign_time = time(NULL);
      alarm->expiration_time = time(NULL) + alarm->seconds; // change it later,
      alarm->change_status = 0;                             // false
      alarm->change_group_status = 0;
      alarm->cancel_status = 0;
      alarm->end_status = 0;
      alarm->time =
          time(NULL) + alarm->seconds; // change it later, will be changed in
                                       // course of display manipulation

      /*
       * Insert alarm into the list.
       */
      if (insert_alarm_into_list(alarm) == NULL) {
        /*
         * If inserting into alarm returns NULL, then the
         * alarm was not inserted into this list. In this
         * case, unlock the alarm list mutex and free the
         * alarm's memory.
         */
        free(alarm);
        pthread_mutex_unlock(&alarm_mutex);
        continue;
      }

      // In this case, the alarm has been successfully added, and the message is
      // printed by the
      // main thread accordingly.

      printf("Alarm(%d) Inserted by Main Thread(%d) Into Alarm List at "
             "%ld:Group(%d) %d "
             "%s\n",
             alarm->alarm_id, MAIN_THREAD_ID, time(NULL), alarm->group_id,
             alarm->seconds, alarm->message);

      // Signal to any display thread having available space that a new alarm
      // has been added.
      // If no display thread exists, then we create one as shown below
      DEBUG_PRINT("main_thread: Alarm State: %s", "Signal event\n");
      pthread_cond_signal(&alarm_cond);

      /*
       * If all the threads are filled out,
       we create a new thread for the new alarm
       */
      if (alarm_groupID_exist_in_thread(alarm) ==
          0) { // True if no thread is available for the new alarm

        // Create and assign space for a new thread.
        next_thread = malloc(sizeof(thread_t));
        if (next_thread == NULL) {
          errno_abort("Malloc failed");
        }
        // Fill in data of thread
        DEBUG_PRINT("main_thread: Alarm State: %s", "Thread fill event\n");
        next_thread->thread_id = thread_id_counter;
        next_thread->alarm_counter = 1;
        next_thread->next = NULL;
        // next_thread->alarm1 = alarm;
        // next_thread->alarm2 = NULL;
        next_thread->group_id = alarm->group_id;

        // printf("Thread type is %s\n", next_thread->type);
        // Increment thread ID counter
        thread_id_counter++;

        /*
        Add the thread to the thread list.
        As a first step, we lock the mutex to prevent any synchronization error
        */
        pthread_mutex_lock(&thread_mutex);
        add_to_thread_list(next_thread);
        DEBUG_PRINT("main_thread: Thread State: %s", "Add thread event\n");
        pthread_mutex_unlock(&thread_mutex); // Unlock mutex once operation is
                                             // performed successively.

        // Create the new thread.
        pthread_create(&next_thread->thread, NULL, alarm_thread, next_thread);

        // set debugger here

        // No alarms of the type of current alarm exist in a full thread
        // In this case, we print an alarm as a new display
        DEBUG_PRINT("main_thread: Thread State: %s with Display Thread ID %d\n",
                    "Printing new display Alarm thread event",
                    next_thread->thread_id);

                    

        printf("Alarm Group Display Creation Thread Created New Display Alarm "
               "Thread(%d) For Alarm(%d) at %ld: Group(%d) %d %s\n",
               next_thread->thread_id, alarm->alarm_id, time(NULL),
               alarm->group_id, alarm->seconds, alarm->message);
                    
                    time_t assign_time= time(NULL);
                    alarm->assign_time = assign_time;
      }

      else {

        /*
         * Arriving here means there is atleast one thread with an available
         * space which is of the same group ID as the alarm which is to be
         * added. We just assign the alarm to the thread and we are done
         */

        // Update the thread with a new alarm
        pthread_mutex_lock(&thread_mutex);

        thread_t *current_thread = thread_header.next;

        while (current_thread != NULL) {
          if (current_thread->group_id == alarm->group_id) {

            current_thread->alarm_counter++;
            DEBUG_PRINT(
                "main_thread: Thread State: %s with Display Thread ID %d\n",
                "Assigning alarm to existing thread event",
                current_thread->thread_id);

               
            printf("Alarm Thread Display Alarm Thread(%d) Assigned to Display "
                   "Alarm(%d) at %ld: Group(%d) %d %s\n",
                   current_thread->thread_id, alarm->alarm_id, time(NULL),
                   current_thread->group_id, alarm->seconds, alarm->message);
                
                time_t assign_time= time(NULL);
                alarm->assign_time = assign_time;


            break; // Once we identify the available thread, assign to alarm and
                   // break
          }
          current_thread = current_thread->next;
        }

        pthread_mutex_unlock(&thread_mutex);
        DEBUG_PRINT("Thread mutex is %s\n", "Unlocked");
        pthread_mutex_unlock(&alarm_mutex);
        DEBUG_PRINT("Alarm mutex is %s\n", "Unlocked");
        continue;
      }

      /*
       * Unlock the mutex now since we are done with this section
       */
      pthread_mutex_unlock(&alarm_mutex);
      continue;
    }

    else if (strncmp(alarmR, "Change_Alarm", 12) == 0) {
      
      DEBUG_PRINT("main_thread: Input State: %s", "Change event\n");
      // We adjust the alarm type formatting before applying change function

      DEBUG_PRINT("Group ID for change event is %d\n", alarm->group_id);
      // printf("New type %s\n", alarm->type);
      // We call change function to handle alarm change accordingly.

      alarm_t *finder = search_alarm_by_id(alarm->alarm_id);
      if (finder == NULL) {
        DEBUG_PRINT("No alarm is present in alarm list with alarm_id %d\n",
                    alarm->alarm_id);
        printf("No alarm is present in alarm list with alarm_id %d\n",
               alarm->alarm_id);
        continue;
      }

 status = pthread_mutex_lock(&alarm_mutex);
      if (status != 0)
        err_abort(status, "Lock mutex");


      time_t change_time = time(NULL);
      finder->expiration_time = change_time + alarm->seconds;

      int i;
      for (i = 0; alarm->message[i] != '\0' && i < sizeof(finder->message) - 1;
           i++)
        finder->message[i] = alarm->message[i];
      finder->message[i] = '\0';

      finder->change_status = 1;

      if (finder->group_id != alarm->group_id) {
        DEBUG_PRINT("%s for Alarm(%d)", "Group Alarm has been changed", alarm->alarm_id);
        finder->change_group_status = 1;
      }

      printf("Alarm(%d) Changed at %ld:Group(%d) %d %s\n", alarm->alarm_id,
             time(NULL), alarm->group_id, alarm->seconds, alarm->message);

      /**ALARM GROUP CHANGE CHECK FOR NEW THREAD
       * We check if the group id has changed alarm, in this case, we
       * assign the new changed alarm to a new thread, and re-insert the new
       * alarm .
       *
       * To prevent the alarm list from having 2 alarms with the same ID, unless
       * we are performing a fast change operation, an additional condition is
       * added when we check for 2 identical ID's
       */
      if (finder->change_group_status == 1) {

        // Entering here implies group of alarm has been changed. We would
        // therefore perform the necessary changes.

    

        /*
         * Fill in data for alarm.
         */
      

        DEBUG_PRINT("main_thread: Alarm State: %s", "Alarm Data filling\n");
        alarm->status = 1; // true
        alarm->creation_time = finder->creation_time;
        alarm->assign_time= time(NULL);
        alarm->expiration_time = time(NULL) + alarm->seconds; // change it later,
        alarm->change_status = 1;        // false
        alarm->change_group_status = 1;
        alarm->cancel_status = 0;
        alarm->end_status = 0;
        alarm->time =
            time(NULL) + alarm->seconds; // change it later, will be changed in
                                         // course of display manipulation


        /*
         * Insert alarm into the list.
         */
        if (insert_alarm_into_list(alarm) == NULL) {
          /*
           * If inserting into alarm returns NULL, then the
           * alarm was not inserted into this list. In this
           * case, unlock the alarm list mutex and free the
           * alarm's memory.
           */
          printf("Alarm is null\n");
          free(alarm);
          pthread_mutex_unlock(&alarm_mutex);
          continue;
        }
     
        // In this case, the alarm has been successfully added, and the message
        // is printed by the main thread accordingly.

        alarm->change_group_status, alarm->change_status = 0;

        // Signal to any display thread having available space that the group_id
        // of an alarm has been changed. If no display thread exists, then we
        // create one as shown below
        DEBUG_PRINT("main_thread: Alarm State: %s", "Signal event\n");
        
        pthread_cond_signal(&alarm_cond);
      
   
        /*
        * If all the threads are filled out,
        we create a new thread for the new alarm
        */
        if (alarm_groupID_exist_in_thread(alarm) ==
            0) { // True if no thread is available for the new alarm

          // Create and assign space for a new thread.
          next_thread = malloc(sizeof(thread_t));
          if (next_thread == NULL) {
            errno_abort("Malloc failed");
          }
          // Fill in data of thread
          DEBUG_PRINT("main_thread: Alarm State: %s", "Thread fill event\n");
          next_thread->thread_id = thread_id_counter;
          next_thread->alarm_counter = 1;
          next_thread->next = NULL;
          next_thread->group_id = alarm->group_id;

          // printf("Thread type is %s\n", next_thread->type);
          // Increment thread ID counter
          thread_id_counter++;

          /*
          Add the thread to the thread list.
          As a first step, we lock the mutex to prevent any synchronization
          error
          */
          pthread_mutex_lock(&thread_mutex);
          add_to_thread_list(next_thread);
          DEBUG_PRINT("main_thread: Thread State: %s", "Add thread event\n");
          pthread_mutex_unlock(&thread_mutex); // Unlock mutex once operation is
                                               // performed successively.

          // Create the new thread.
          pthread_create(&next_thread->thread, NULL, alarm_thread, next_thread);

          // set debugger here

          // print the changed display message
          printf("Display Thread(%d) Has Taken Over Printing Message of "
                 "Alarm(%d) at %ld: Changed Group(%d) %d %s\n",
                 next_thread->thread_id, alarm->alarm_id, time(NULL),
                 next_thread->group_id, alarm->seconds, alarm->message);

                 time_t assign_time= time(NULL);
                alarm->assign_time = assign_time;

          // No alarms of the type of current alarm exist in a full thread
          // In this case, we print an alarm as a new display
          DEBUG_PRINT("main_thread: Thread State: %s %d with Display Thread ID "
                      "%d for changed Group %d\n",
                      "Printing new display Alarm thread event with alarm",
                      alarm->alarm_id, next_thread->thread_id,
                      next_thread->group_id);

        }

        else {
          /*
           * Arriving here means there is atleast one thread with an available
           * space which is of the same group ID as the alarm which is to be
           * added. We just assign the alarm to the thread and we are done
           */

          // Update the thread with a new alarm
          pthread_mutex_lock(&thread_mutex);

          thread_t *current_thread = thread_header.next;

          while (current_thread != NULL) {
            if (current_thread->group_id == alarm->group_id) {

              current_thread->alarm_counter++;
              DEBUG_PRINT("main_thread: Thread State: %s with Display Thread "
                          "ID %d having thread display group %d\n",
                          "Assigning alarm to existing thread event",
                          next_thread->thread_id, next_thread->group_id);

              // print the changed display message
              printf("Display Thread(%d) Has Taken Over Printing Message of "
                     "Alarm(%d) at %ld: Changed Group(%d) %d %s\n",
                     next_thread->thread_id, alarm->alarm_id, time(NULL),
                     next_thread->group_id, alarm->seconds, alarm->message);

                     time_t assign_time= time(NULL);
                   alarm->assign_time = assign_time;

              break; // Once we identify the available thread, assign to alarm
                     // and break
            }
            current_thread = current_thread->next;
          }

          pthread_mutex_unlock(&thread_mutex);
          DEBUG_PRINT("Thread mutex is %s\n", "Unlocked");
         
          // pthread_mutex_unlock(&alarm_mutex);
          // DEBUG_PRINT("Alarm mutex is %s\n", "Unlocked");
          // continue;
        }
     

      }
         
         pthread_mutex_unlock(&alarm_mutex);
          DEBUG_PRINT("Alarm mutex is %s\n", "Unlocked");
          continue;
      // continue;
    }

    else {
      DEBUG_PRINT("Command is not in appropriate %s\n", "Format");
      fprintf(stderr, "Bad command Format\n");
    }

    // set debugger here
    
    /*
     * We are done updating the list, so notify the other
     * threads by broadcasting, then unlock the mutex so that
     * the other threads can lock it.
     */
    pthread_cond_broadcast(&alarm_cond);
    pthread_mutex_unlock(&alarm_mutex);
    free(alarm);
    
    // continue;
  }

  kill_sem();
 

  return 0;
}
