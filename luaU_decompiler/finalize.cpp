#include "finalize.hpp"
#include <iostream>

#define spacing "   "


#define compare_1 "elseif"
#define compare_2 "else"
#define compare_3 "if"
#define compare_4 "repeat"
#define compare_5 "function"
#define compare_6 "local function"
#define compare_7 "for"
#define compare_8 "while"
#define compare_9 "(function"

#define compare_end "end"
#define compare_until "until"


void finalize_decompilation(std::string& write) {
	
	const auto spaces = std::string(spacing);
	const auto s_size = spaces.length();
	
	/* How many indentations we currently want. */
	std::uint64_t multiplier = 0u;

	/* Makes sures it isn't in string. */
	bool clean = true;

	/* Add line ending at the front to signal compare. */
	write.insert(write.begin(), '\n');

	for (auto pos = 0u; pos < write.length(); ++pos) 
	{
		const auto ch = write[pos];

		if (ch == '\"' || ch == '\'')
			clean ^= true;

		/* See if new line and were not in a string. */
		if (ch == '\n' && clean) 
		{
			/* Check if, "if" or "repeat", etc, exists.  if so increase multiplier. */
			if (!write.compare(pos + 1u, (sizeof(compare_6) - 1u), compare_6) || !write.compare(pos + 1u, (sizeof(compare_5) - 1u), compare_5) ||
				!write.compare(pos + 1u, (sizeof(compare_3) - 1u), compare_3) || !write.compare(pos + 1u, (sizeof(compare_4) - 1u), compare_4) || 
				!write.compare(pos + 1u, (sizeof(compare_7) - 1u), compare_7) || !write.compare(pos + 1u, (sizeof(compare_8) - 1u), compare_8) ||
				!write.compare(pos + 1u, (sizeof(compare_9) - 1u), compare_9))
				++multiplier;
			else if ((!write.compare(pos + 1u, (sizeof(compare_end) - 1u), compare_end) || !write.compare(pos + 1u, (sizeof(compare_until) - 1u), compare_until)) && multiplier)
				--multiplier;

			/* Reverse format. */
			if (!write.compare(pos + 1u, (sizeof(compare_9) - 1u), compare_9))
				write.erase(pos, 1u);

			/* Make sure elseif and else has -1 else doe format as per usual. */
			if (!write.compare(pos + 1u, (sizeof(compare_9) - 1u), compare_9) ||
				!write.compare(pos + 1u, (sizeof(compare_8) - 1u), compare_8) ||
				!write.compare(pos + 1u, (sizeof(compare_7) - 1u), compare_7) ||
				!write.compare(pos + 1u, (sizeof(compare_6) - 1u), compare_6) ||
				!write.compare(pos + 1u, (sizeof(compare_5) - 1u), compare_5) ||
				!write.compare(pos + 1u, (sizeof(compare_4) - 1u), compare_4) ||
				!write.compare(pos + 1u, (sizeof(compare_3) - 1u), compare_3) ||
				!write.compare(pos + 1u, (sizeof(compare_2) - 1u), compare_2) ||
				!write.compare(pos + 1u, (sizeof(compare_1) - 1u), compare_1)) 

				for (auto o = 0u; o < (multiplier - 1u); ++o)
					write.insert(pos + 1, spaces);
			else
				for (auto o = 0u; o < multiplier; ++o) 
					write.insert(pos + 1, spaces);
		}
	}

	/* Remove finalizing line ending. */
	write.erase(write.begin());

	return;
}