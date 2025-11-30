AggDraw
=======

The AggDraw library provides a python interface on top of
`the AGG library <http://www.antigrain.com/>`_. The library was originally
developed by Fredrik Lundh (effbot), but has since been picked up by various
developers. It is currently maintained by the PyTroll developmer group. The
official repository can be found on GitHub:

https://github.com/pytroll/aggdraw

The original documentation by effbot is
`still available <http://effbot.org/zone/aggdraw-index.htm>`_ but may be out
of date with the current version of the library. Original examples will be
migrated as time is available (pull requests welcome).

Installation
------------

Aggdraw is available on Linux, OSX, and Windows. It can be installed from PyPI
with pip:

.. code-block:: bash

    pip install aggdraw

Or from conda with the conda-forge channel:

.. code-block:: bash

    conda install -c conda-forge aggdraw

Free-threading support
----------------------

Basic free-threading compatibility has been enabled starting with the Python
3.14 wheels of aggdraw 1.4.0. However, only the minimum steps have been taken
to let Python 3.14's free-threaded build know that aggdraw could be used
without the GIL. No additional locking or redesign of the library has been
done.

Aggdraw itself should have no global state, but each individual object is free
to have internal state. It is up to the user to limit interactions for an
aggdraw object to a single thread or to protect against concurrent access
using locks. Even with this in mind, free-threading support in aggdraw is
extremely unstable and experimental. If you have a use case for aggdraw
in a free-threading environment please file an issue on GitHub to describe
your use case and how it is going.

API
---

.. automodule:: aggdraw
    :members:
    :undoc-members:
    :show-inheritance:


Indices and tables
------------------

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
