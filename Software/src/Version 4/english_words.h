#ifndef ENGLISH_WORDS_H
#define ENGLISH_WORDS_H

/// The most common English Words in various lengths for CW Trainer
///////////////////////////////////////////////////////////////////

namespace EnglishWords
  {
  const int WORDS_NUMBER_OF_ELEMENTS = 373;                           // how many items all together?
  const int WORDS_MAX_SIZE = 14;                                      // longest item  +1
  const int WORDS_POINTER[] = {0, 370, 345, 275, 144, 66, 34, 14};    // array where items start with length = index
  
  const String words[WORDS_NUMBER_OF_ELEMENTS]  = { 
    { "international" },
    { "university" },
    { "government" },
    { "including" },
    { "following" },
    { "national" },
    { "american" },
    { "released" },
    { "although" },
    { "district" },
    { "sentence" },    /// new
    { "together" },    /// new
    { "children" },    /// new
    { "mountain" },    /// new
    { "between" },        /// l = 7, pos = 14
    { "however" },
    { "through" },
    { "several" },
    { "history" },
    { "against" },
    { "because" },
    { "located" },
    { "company" },
    { "general" },
    { "another" },
    { "century" },
    { "station" },
    { "british" },
    { "college" },
    { "members" },
    { "picture" },    /// new
    { "country" },    /// new
    { "thought" },    /// new
    { "example" },    /// new
    { "during" },       /// l = 6, pos = 34
    { "school" },
    { "united" },
    { "states" },
    { "became" },
    { "before" },
    { "people" },
    { "second" },
    { "called" },
    { "series" },
    { "number" },
    { "family" },
    { "county" },
    { "system" },
    { "season" },
    { "played" },
    { "around" },
    { "public" },
    { "former" },
    { "career" },
    { "little" },    /// new
    { "differ" },    /// new
    { "follow" },    /// new
    { "change" },    /// new
    { "animal" },    /// new
    { "mother" },    /// new
    { "father" },    /// new
    { "should" },    /// new
    { "answer" },    /// new
    { "always" },    /// new
    { "letter" },    /// new
    { "friend" },    /// new
    { "which" },      /// l = 5, pos = 66
    { "first" },
    { "their" },
    { "after" },
    { "other" },
    { "there" },
    { "years" },
    { "would" },
    { "where" },
    { "later" },
    { "these" },
    { "about" },
    { "under" },
    { "world" },
    { "known" },
    { "while" },
    { "state" },
    { "three" },
    { "being" },
    { "early" },
    { "since" },
    { "until" },
    { "south" },
    { "north" },
    { "music" },
    { "album" },
    { "group" },
    { "often" },
    { "those" },
    { "house" },
    { "began" },
    { "could" },
    { "found" },
    { "major" },
    { "river" },
    { "named" },
    { "still" },
    { "place" },
    { "local" },
    { "party" },
    { "large" },
    { "small" },
    { "along" },
    { "based" },
    { "write" },    /// new
    { "thing" },    /// new
    { "sound" },    /// new
    { "water" },    /// new
    { "round" },    /// new
    { "every" },    /// new
    { "great" },    /// new
    { "think" },    /// new
    { "cause" },    /// new
    { "right" },    /// new
    { "spell" },    /// new
    { "light" },    /// new
    { "again" },    /// new
    { "point" },    /// new
    { "build" },    /// new
    { "earth" },    /// new
    { "stand" },    /// new
    { "study" },    /// new
    { "learn" },    /// new
    { "plant" },    /// new
    { "cover" },    /// new
    { "never" },    /// new
    { "cross" },    /// new
    { "start" },    /// new
    { "might" },    /// new
    { "story" },    /// new
    { "press" },    /// new
    { "close" },    /// new
    { "night" },    /// new
    { "white" },    /// new
    { "begin" },    /// new
    { "paper" },    /// new
    { "carry" },    /// new
    { "watch" },    /// new
    { "with" },     /// l = 4, pos = 144
    { "that" },
    { "from" },
    { "were" },
    { "this" },
    { "also" },
    { "have" },
    { "they" },
    { "been" },
    { "when" },
    { "into" },
    { "more" },
    { "time" },
    { "most" },
    { "some" },
    { "only" },
    { "over" },
    { "many" },
    { "such" },
    { "used" },
    { "city" },
    { "then" },
    { "than" },
    { "made" },
    { "part" },
    { "year" },
    { "both" },
    { "them" },
    { "name" },
    { "area" },
    { "well" },
    { "will" },
    { "high" },
    { "born" },
    { "work" },
    { "town" },
    { "film" },
    { "team" },
    { "each" },
    { "life" },
    { "same" },
    { "game" },
    { "four" },
    { "west" },
    { "line" },
    { "like" },
    { "very" },
    { "john" },
    { "home" },
    { "back" },
    { "band" },
    { "show" },
    { "york" },
    { "even" },
    { "much" },
    { "east" },
    { "what" }, /// new
    { "your" }, /// new
    { "word" }, /// new
    { "said" }, /// new
    { "long" }, /// new
    { "make" }, /// new
    { "look" }, /// new
    { "come" }, /// new
    { "know" }, /// new
    { "call" }, /// new
    { "down" }, /// new
    { "side" }, /// new
    { "find" }, /// new
    { "take" }, /// new
    { "live" }, /// new
    { "came" }, /// new
    { "good" }, /// new
    { "give" }, /// new
    { "just" }, /// new
    { "form" }, /// new
    { "help" }, /// new
    { "turn" }, /// new
    { "mean" }, /// new
    { "move" }, /// new
    { "does" }, /// new
    { "tell" }, /// new
    { "want" }, /// new
    { "play" }, /// new
    { "read" }, /// new
    { "hand" }, /// new
    { "port" }, /// new
    { "land" }, /// new
    { "here" }, /// new
    { "must" }, /// new
    { "went" }, /// new
    { "kind" }, /// new
    { "need" }, /// new
    { "near" }, /// new
    { "self" }, /// new
    { "head" }, /// new
    { "page" }, /// new
    { "grow" }, /// new
    { "food" }, /// new
    { "keep" }, /// new
    { "last" }, /// new
    { "door" }, /// new
    { "tree" }, /// new
    { "hard" }, /// new
    { "draw" }, /// new
    { "left" }, /// new
    { "late" }, /// new
    { "real" }, /// new
    { "stop" }, /// new
    { "open" }, /// new
    { "seem" }, /// new
    { "next" }, /// new
    { "walk" }, /// new
    { "ease" }, /// new
    { "mark" }, /// new
    { "book" }, /// new
    { "mile" }, /// new
    { "feet" }, /// new
    { "care" }, /// new
    { "took" }, /// new
    { "rain" }, /// new
    { "room" }, /// new
    { "idea" }, /// new
    { "fish" }, /// new
    { "once" }, /// new
    { "base" }, /// new
    { "hear" }, /// new
    { "sure" }, /// new
    { "face" }, /// new
    { "wood" }, /// new
    { "main" }, /// new
    { "the" },      /// l = 3, pos = 275
    { "and" },
    { "was" },
    { "for" },
    { "his" },
    { "are" },
    { "has" },
    { "had" },
    { "one" },
    { "not" },
    { "but" },
    { "its" },
    { "new" },
    { "who" },
    { "her" },
    { "two" },
    { "she" },
    { "all" },
    { "can" },
    { "may" },
    { "out" },
    { "him" },
    { "war" },
    { "age" },
    { "now" },
    { "use" },
    { "any" },
    { "end" },
    { "day" },
    { "did" },
    { "own" },
    { "due" },
    { "won" },
    { "sum" },  // 
    { "usa" },  // 
    { "you" }, /// new
    { "hot" }, /// new
    { "how" }, /// new
    { "way" }, /// new
    { "see" }, // new
    { "man" }, // new
    { "our" }, // new
    { "say" }, // new
    { "low" }, // new
    { "boy" }, // new
    { "old" }, // new
    { "too" }, // new
    { "set" }, // new
    { "air" }, // new
    { "put" }, // new
    { "add" }, // new
    { "big" }, // new
    { "act" }, // new
    { "why" }, // new
    { "ask" }, // new
    { "men" }, // new
    { "off" }, // new
    { "try" }, // new
    { "sun" }, // new
    { "let" }, // new
    { "eye" }, // new
    { "saw" }, // new
    { "far" }, // new
    { "sea" }, // new
    { "run" }, // new
    { "few" }, // new
    { "got" }, // new
    { "car" }, // new
    { "eat" }, // new
    { "cut" }, // new
    { "of" },     /// l = 2, pos = 345
    { "km" },   // 
    { "mr" },   // 
    { "us" },   // 
    { "in" },
    { "to" },
    { "is" },
    { "as" },
    { "on" },
    { "by" },
    { "he" },
    { "at" },
    { "it" },
    { "an" },
    { "or" },
    { "be" },
    { "up" },
    { "no" },
    { "so" },
    { "if" },
    { "we" }, // new
    { "do" }, // new
    { "go" }, // new
    { "my" }, // new
    { "me" }, // new
    { "a" },  // 370
    { "i" },   /// 
    { "m" }     // pos = 372
    };
  }
#endif
