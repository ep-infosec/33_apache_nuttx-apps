/****************************************************************************
 * apps/nshlib/nsh_login.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "fsutils/passwd.h"
#ifdef CONFIG_NSH_CLE
#  include "system/cle.h"
#else
#  include "system/readline.h"
#endif

#include "nsh.h"
#include "nsh_console.h"

#ifdef CONFIG_NSH_CONSOLE_LOGIN

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_token
 ****************************************************************************/

static void nsh_token(FAR struct console_stdio_s *pstate,
                      FAR char *buffer, size_t buflen)
{
  FAR char *start;
  FAR char *endp1;
  bool quoted = false;

  /* Find the start of token.  Either (1) the first non-white space
   * character on the command line or (2) the character immediately after
   * a quotation mark.
   */

  for (start = pstate->cn_line; *start; start++)
    {
      /* Does the token open with a quotation mark */

      if (*start == '"')
        {
          /* Yes.. break out with start set to the character after the
           * quotation mark.
           */

          quoted = true;
          start++;
          break;
        }

      /* No, then any non-whitespace is the first character of the token */

      else if (!isspace(*start))
        {
          /* Break out with start set to the first character of the token */

          break;
        }
    }

  /* Find the terminating character after the token on the command line.  The
   * terminating character is either (1) the matching quotation mark, or (2)
   * any whitespace.
   */

  for (endp1 = start; *endp1; endp1++)
    {
      /* Did the token begin with a quotation mark? */

      if (quoted)
        {
          /* Yes.. then only the matching quotation mark (or end of string)
           * terminates
           */

          if (*endp1 == '"')
            {
              /* Break out... endp1 points to closing quotation mark */

              break;
            }
        }

      /* No.. any whitespace (or end of string) terminates */

      else if (isspace(*endp1))
        {
          /* Break out... endp1 points to first while space encountered */

          break;
        }
    }

  /* Replace terminating character with a NUL terminator */

  *endp1 = '\0';

  /* Copied the token into the buffer */

  strncpy(buffer, start, buflen);
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: nsh_login
 *
 * Description:
 *   Prompt the user for a username and password.  Return a failure if valid
 *   credentials are not returned (after some retries.
 *
 ****************************************************************************/

int nsh_login(FAR struct console_stdio_s *pstate)
{
  char username[16];
  char password[128];
#ifdef CONFIG_NSH_PLATFORM_CHALLENGE
  char challenge[128];
#endif
  int ret;
  int i;

#ifdef CONFIG_NSH_PLATFORM_SKIP_LOGIN
  if (platform_skip_login() == OK)
    {
      return OK;
    }
#endif

  /* Loop for the configured number of retries */

  for (i = 0; i < CONFIG_NSH_LOGIN_FAILCOUNT; i++)
    {
      /* Ask for the login username */

      username[0] = '\0';
      fputs(g_userprompt, pstate->cn_outstream);
      fflush(pstate->cn_outstream);

      /* readline() returns EOF on failure */

      ret = readline(pstate->cn_line, CONFIG_NSH_LINELEN,
                     INSTREAM(pstate), OUTSTREAM(pstate));
      if (ret != EOF)
        {
          /* Parse out the username */

          nsh_token(pstate, username, sizeof(username));
        }

      if (username[0] == '\0')
        {
          i--;
          continue;
        }

#ifdef CONFIG_NSH_PLATFORM_CHALLENGE
      platform_challenge(challenge, sizeof(challenge));
      fputs(challenge, pstate->cn_outstream);
      fflush(pstate->cn_outstream);
#endif

      /* Ask for the login password */

      fputs(g_passwordprompt, pstate->cn_outstream);
      fflush(pstate->cn_outstream);

      password[0] = '\0';
      if (fgets(pstate->cn_line, CONFIG_NSH_LINELEN,
                INSTREAM(pstate)) != NULL)
        {
          /* Parse out the password */

          nsh_token(pstate, password, sizeof(password));

          /* Verify the username and password */

#if defined(CONFIG_NSH_LOGIN_PASSWD)
          ret = passwd_verify(username, password);
          if (PASSWORD_VERIFY_MATCH(ret))

#elif defined(CONFIG_NSH_LOGIN_PLATFORM)
#ifdef CONFIG_NSH_PLATFORM_CHALLENGE
          ret = platform_user_verify(username, challenge, password);
#else
          ret = platform_user_verify(username, password);
#endif
          if (PASSWORD_VERIFY_MATCH(ret))

#elif defined(CONFIG_NSH_LOGIN_FIXED)
          if (strcmp(password, CONFIG_NSH_LOGIN_PASSWORD) == 0 &&
              strcmp(username, CONFIG_NSH_LOGIN_USERNAME) == 0)
#else
#  error No user verification method selected
#endif
            {
              fputs(g_loginsuccess, pstate->cn_outstream);
              fflush(pstate->cn_outstream);
              return OK;
            }
          else
            {
              fputs(g_badcredentials, pstate->cn_outstream);
              fflush(pstate->cn_outstream);
#if CONFIG_NSH_LOGIN_FAILDELAY > 0
              usleep(CONFIG_NSH_LOGIN_FAILDELAY * 1000L);
#endif
            }
        }
    }

  /* Too many failed login attempts */

  fputs(g_loginfailure, pstate->cn_outstream);
  fflush(pstate->cn_outstream);
  return -1;
}

#endif /* CONFIG_NSH_CONSOLE_LOGIN */
