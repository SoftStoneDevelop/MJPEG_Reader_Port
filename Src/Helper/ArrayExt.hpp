namespace ArrayExt
{
    /// <summary>
    /// Find first equal bytes in source
    /// </summary>
    /// <returns>-1 if not find in source</returns>
    template<typename T>
    int FindBytesIndex(
        T* source,
        const int& sourceSize,
        const T* pattern,
        const int& patternSize
    )
    {
        int index = -1;
        for (int i = 0; i < sourceSize; i++)
        {
            if (sourceSize - i < patternSize)
            {
                return index;
            }

            for (int j = 0; j < patternSize; j++)
            {
                if (source[i + j] == pattern[j])
                {
                    if (j == patternSize - 1)
                    {
                        index = i;
                        return index;
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    break;
                }
            }
        }

        return index;
    }
}