package metrics;

import com.google.common.base.Preconditions;
import org.apache.commons.lang3.RandomStringUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

public class ListFactory {
    private static final int KS_LOWER = 20;
    private static final int KS_UPPER = 201;
    private static final int VS_LOWER = 200;
    private static final int VS_UPPER = 2001;

    private static final Random rand = new Random();

    private static int randNum(int lower, int upper) {
        return rand.nextInt(upper-lower) + lower;
    }

    public static List<KV> create(int num) {
        Preconditions.checkArgument(num > 0);

        List<KV> list = new ArrayList<>(num);

        for (int i = 0; i < num; ++i) {
            String key = RandomStringUtils.randomAlphanumeric(randNum(KS_LOWER, KS_UPPER));
            String val = RandomStringUtils.randomAlphanumeric(randNum(VS_LOWER, VS_UPPER));
            list.add(new KV(key, val));
        }

        return list;
    }
}

