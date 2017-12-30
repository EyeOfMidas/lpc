/* note that the default _have_ to be zero for flags,
 * it simplifies mapping handling of flags a lot
 * this will be moved to soul.h or something similar.
 */

/* say, scream etc. etc. */
#define FLAG_SOUND 1

/* hit, spit, kick etc. etc. */
#define FLAG_OFFENSE 2

/* kiss, hug, etc. etc. */
#define FLAG_TOUCH 4


/* This is how a CONDITION structure looks like, it is heavily used by
 * reduce_verb
 * ({
 *   metaverb,
 *   verb,
 *   verbdata,
 *   ({ who }),       // the persons who were mentioned
 *   ({ adverbs })    // the adverbs used
 *   ({ adjectives }) // adjectives used
 *   ({ messages }),  // messages given 
 *   ({ bodyparts }), // bodyparts given
 * })

 * cond may be expanded in the future...
 */

#define C_METAVERB 0
#define C_VERB 1
#define C_DATA 2
#define C_WHO 3
#define C_ADV 4
#define C_ADJ 5
#define C_MSGS 6
#define C_BODY 7

