#pragma once

/// Source location (line and column).
struct ow_source_location {
	unsigned int line, column;
};

/// Source range (location of begin and end).
struct ow_source_range {
	struct ow_source_location begin, end;
};
