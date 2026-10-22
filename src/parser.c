#include "common.h"

void parse_line(const char *line, struct ParsedLine *result)
{
    result->count = 0;

    int i = 0;
    while (line[i] != '\0' && result->count < MAX_TOKENS && i < MAX_LINE)
    {
        while (line[i] == ' ' || line[i] == '\t')
            i++;

        if (line[i] == '\0')
            break;

        int pos = 0;
        char quote = '\0';

        if (line[i] == '"' || line[i] == '\'')
        {
            quote = line[i];
            i++;
        }

        while (line[i] != '\0' && pos < PATH_MAX - 1 && i < MAX_LINE)
        {
            if (quote)
            {
                if (line[i] == quote)
                {
                    i++;
                    break;
                }
                result->chars[result->count][pos++] = line[i++];
            }
            else
            {
                if (line[i] == ' ' || line[i] == '\t')
                    break;
                result->chars[result->count][pos++] = line[i++];
            }
        }

        result->chars[result->count][pos] = '\0';
        if (pos > 0)
            result->count++;
    }
}
