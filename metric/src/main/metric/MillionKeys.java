package metric;

import com.google.common.base.Preconditions;
import org.apache.commons.lang3.RandomStringUtils;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Random;


class MillionKeys implements Iterator<String> {
    private final int ONE_MILLION = 1000000;
    private final Random rand = new Random();
    private final List<String> prefixs;
    private int indexPrefix;
    private int indexInOnePrefix;

    public MillionKeys(int howManyMillion) {
        this.prefixs = new ArrayList<>();
        for (int i = 0; i < howManyMillion; ++i) {
            String prefix = RandomStringUtils.randomAlphanumeric(randPrefixLen());
            this.prefixs.add(prefix);
        }
        resetIterator();
    }

    void resetIterator() {
        this.indexPrefix = 0;
        this.indexInOnePrefix = -1;
    }

    @Override
    public boolean hasNext() {
        if (indexPrefix == prefixs.size()) {
            return false;
        }

        ++indexInOnePrefix;
        if (indexInOnePrefix < ONE_MILLION) {
            return true;
        } else {
            indexInOnePrefix = 0;
            ++indexPrefix;
            return indexPrefix != prefixs.size();
        }
    }

    @Override
    public String next() {
        return this.prefixs.get(this.indexPrefix) + alignMillionInt(this.indexInOnePrefix);
    }

    String randValue() {
        return RandomStringUtils.randomAlphanumeric(randInt(200, 2000));
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

    String randomIntToStr() {
        int r = this.rand.nextInt(ONE_MILLION);
        return alignMillionInt(r);
    }

    /* Because we use 6 char as 1 million suffix, so 20-200 key random'prefix is from 14-194 */
    private int randPrefixLen() {
        return randInt(14, 194);
    }

}
