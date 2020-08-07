
/**
 ** Written by Amit Sides
 **/

#ifndef MULTITHREADING_COLORS_H
#define MULTITHREADING_COLORS_H

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"

#define BOLD_BLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLD_RED     "\033[1m\033[31m"      /* Bold Red */
#define BOLD_GREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLD_YELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLD_BLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLD_MAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLD_CYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLD_WHITE   "\033[1m\033[37m"      /* Bold White */

#define RESET "\x1B[0m"

const char *colors[] = {GRN, YEL, BLU, RED, CYN, MAG};

#endif //MULTITHREADING_COLORS_H
