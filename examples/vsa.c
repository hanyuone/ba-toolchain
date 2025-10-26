int main() {
      int *part1, *part2;
      int a[10], *p_array0;
      int i;

      part1 = &a[0];
      p_array0 = part1;
      part2 = &a[5];

      for (i = 0; i < 5; ++i) {
          *part1 = 0;
          *part2 = 1;
          part1++;
          part2++;
      }

      return *p_array0;
}
