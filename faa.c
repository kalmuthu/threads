/*
 * faa.c
 *
 *  Created on: Apr 15, 2015
 *      Author: vagrant
 */

/**
 * @brief Implementation of fetch and add
 * @see Taken from wikipedia: https://www.en.wikipedia.org/wiki/Fetch-and-add
 * @param variable The variable to modify
 * @param value The value to modify
 * @return The updated variabled
 */
inline int fetch_and_add(unsigned int * variable, int value ) {
  asm volatile("lock; xaddl %%eax, %2;"
			   :"=a" (value)                  //Output
			   :"a" (value), "m" (*variable)  //Input
			   :"memory");
  return value;
}
