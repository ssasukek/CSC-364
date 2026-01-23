1. 4 threads doing two stage tone mapping on a BMP function
2. Implement two barriers and show both work:
    a. sense reversing barriers
    b. DIY (dont use a built in barrier)
        - gate barrier uses two shared atomics - count and gate

Sense reversing barrier (counter + global sense + per thread local sense)
    - to avoids the "stuck in old barrier phase"


Program:
1. loads a bmp image (input)
2. run two stage tone mapping on 4 threads in parallel
3. use gather/barrier between stage 1 and 2 and end
4. saves the tone mapped bmp (output)
5. can run in two sync mode - diy gate barrier and sense reversing barrier


