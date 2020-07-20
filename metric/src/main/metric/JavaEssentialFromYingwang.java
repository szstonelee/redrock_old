package metric;

// check https://www.yinwang.org/blog-cn/2020/02/13/java-type-system
public class JavaEssentialFromYingwang {
    public static void f() {
        String[] a = new String[2];
        Object[] b = a;
        a[0] = "hi";
        b[1] = Integer.valueOf(42);
    }
}
