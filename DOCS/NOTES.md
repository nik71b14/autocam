The voxelized object has been normalized during voxelization:

Original: 100 x 100 x 50

Normalized: 1 x 1 x 0.5

This normalization factor is:

scale = 1 / max_dim
where max_dim = max(100, 100, 50) = 100 ⇒ scale = 0.01

The voxelization result lives in [0,1] x [0,1] x [0, zSpan] where zSpan = 0.5.

In the gocode viewer, I want to display the object in its original size (100x100x50 units), rather than the normalized form.
Currently it appears tiny because you're drawing it as a 1x1x0.5 object.

