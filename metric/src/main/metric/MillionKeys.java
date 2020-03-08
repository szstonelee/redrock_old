package metric;

import com.google.common.base.Preconditions;
import org.apache.commons.lang3.RandomStringUtils;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Random;


class MillionKeys implements Iterator<String> {
    private final int ONE_MILLION = 1000000;
    private final int LOWER_TTL_SECONDS = 60;
    private final int UPPER_TTL_SECONDS = 60000;    // 1000 minutes

    private final Random rand = new Random();
    private final List<String> prefixs;
    private int indexPrefix;
    private int indexInOnePrefix;

    public MillionKeys(int howManyMillion) {
        Preconditions.checkArgument(howManyMillion > 0);

        this.prefixs = new ArrayList<>();
        for (int i = 0; i < howManyMillion; ++i) {
            String prefix = RandomStringUtils.randomAlphanumeric(randPrefixLen());
            this.prefixs.add(prefix);
        }
        resetIterator();
    }

    void resetIterator() {
        this.indexPrefix = 0;
        this.indexInOnePrefix = 0;
    }

    @Override
    public boolean hasNext() {
        return indexInOnePrefix < ONE_MILLION;
    }

    @Override
    public String next() {
        String res = this.prefixs.get(this.indexPrefix) + alignMillionInt(this.indexInOnePrefix);
        ++indexPrefix;
        if (indexPrefix == prefixs.size()) {
            indexPrefix = 0;
            ++indexInOnePrefix;
        }
        return res;
    }

    String randValue() {
        return RandomStringUtils.randomAlphanumeric(randInt(200, 2000));
    }

    String randKey() {
        int randPrefixIndex = rand.nextInt(prefixs.size());
        int randSuffixIndex = rand.nextInt(ONE_MILLION);
        return prefixs.get(randPrefixIndex) + alignMillionInt(randSuffixIndex);
    }

    int randTtl() {
        return randInt(LOWER_TTL_SECONDS, UPPER_TTL_SECONDS);
    }

    String randomIntToStr() {
        int r = this.rand.nextInt(ONE_MILLION);
        return alignMillionInt(r);
    }

    private String alignMillionInt(int n) {
        Preconditions.checkArgument(n >= 0 && n < ONE_MILLION);

        String str = Integer.toString(n);
        int zeros = 6 - str.length();
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < zeros; ++i) {
            b.append('0');
        }
        return b.toString() + str;
    }

    /* random in [lower, upper] */
    private int randInt(int lower, int upper) {
        Preconditions.checkArgument(upper >= lower);

        return lower + this.rand.nextInt(upper-lower+1);
    }

    /* Because we use 6 char as 1 million suffix, so 20-200 key random'prefix is from 14-194 */
    private int randPrefixLen() {
        return randInt(14, 194);
    }

}
