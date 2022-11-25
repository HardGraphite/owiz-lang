#pragma once

#define ow_round_up_to(round_to, number) \
	(((number) + ((round_to) - 1)) & ~((round_to) - 1))
