# Vertex User Mode Runtime

This is the reference implementation of the Vertex API through the Host Operating System's User Mode API.

It shows how the Vertex API can be implemented in a user mode runtime, and can be used as a starting point for implementing the Vertex API in other plugins.

## Advice

Vertex User Mode Runtime is simply a reference implementation of the Vertex API.

Using this in applications with anti tampering mechanisms will likely cause detection.

It is emphasized that Vertex User Mode Runtime will not try to hide itself from anti tampering mechanisms, and will not try to bypass them.